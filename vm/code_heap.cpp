#include "master.hpp"

namespace factor
{

code_heap::code_heap(cell size)
{
	if(size > (1L << (sizeof(cell) * 8 - 6))) fatal_error("Heap too large",size);
	seg = new segment(align_page(size),true);
	if(!seg) fatal_error("Out of memory in heap allocator",size);
	allocator = new free_list_allocator<heap_block>(size,seg->start);
}

code_heap::~code_heap()
{
	delete allocator;
	allocator = NULL;
	delete seg;
	seg = NULL;
}

void code_heap::write_barrier(code_block *compiled)
{
	points_to_nursery.insert(compiled);
	points_to_aging.insert(compiled);
}

void code_heap::clear_remembered_set()
{
	points_to_nursery.clear();
	points_to_aging.clear();
}

bool code_heap::needs_fixup_p(code_block *compiled)
{
	return needs_fixup.count(compiled) > 0;
}

bool code_heap::marked_p(heap_block *compiled)
{
	return allocator->state.marked_p(compiled);
}

void code_heap::set_marked_p(code_block *compiled)
{
	allocator->state.set_marked_p(compiled);
}

void code_heap::clear_mark_bits()
{
	allocator->state.clear_mark_bits();
}

void code_heap::code_heap_free(code_block *compiled)
{
	points_to_nursery.erase(compiled);
	points_to_aging.erase(compiled);
	needs_fixup.erase(compiled);
	allocator->free(compiled);
}

/* Allocate a code heap during startup */
void factor_vm::init_code_heap(cell size)
{
	code = new code_heap(size);
}

bool factor_vm::in_code_heap_p(cell ptr)
{
	return (ptr >= code->seg->start && ptr <= code->seg->end);
}

/* Compile a word definition with the non-optimizing compiler. Allocates memory */
void factor_vm::jit_compile_word(cell word_, cell def_, bool relocate)
{
	gc_root<word> word(word_,this);
	gc_root<quotation> def(def_,this);

	jit_compile(def.value(),relocate);

	word->code = def->code;

	if(to_boolean(word->pic_def)) jit_compile(word->pic_def,relocate);
	if(to_boolean(word->pic_tail_def)) jit_compile(word->pic_tail_def,relocate);
}

struct word_updater {
	factor_vm *parent;

	explicit word_updater(factor_vm *parent_) : parent(parent_) {}
	void operator()(code_block *compiled, cell size)
	{
		parent->update_word_references(compiled);
	}
};

/* Update pointers to words referenced from all code blocks. Only after
defining a new word. */
void factor_vm::update_code_heap_words()
{
	word_updater updater(this);
	iterate_code_heap(updater);
}

/* After a full GC that did not grow the heap, we have to update references
to literals and other words. */
struct word_and_literal_code_heap_updater {
	factor_vm *parent;

	word_and_literal_code_heap_updater(factor_vm *parent_) : parent(parent_) {}

	void operator()(heap_block *block, cell size)
	{
		parent->update_code_block_words_and_literals((code_block *)block);
	}
};

void factor_vm::update_code_heap_words_and_literals()
{
	word_and_literal_code_heap_updater updater(this);
	code->allocator->sweep(updater);
}

/* After growing the heap, we have to perform a full relocation to update
references to card and deck arrays. */
struct code_heap_relocator {
	factor_vm *parent;

	code_heap_relocator(factor_vm *parent_) : parent(parent_) {}

	void operator()(code_block *block, cell size)
	{
		parent->relocate_code_block(block);
	}
};

void factor_vm::relocate_code_heap()
{
	code_heap_relocator relocator(this);
	code_heap_iterator<code_heap_relocator> iter(relocator);
	code->allocator->sweep(iter);
}

void factor_vm::primitive_modify_code_heap()
{
	gc_root<array> alist(dpop(),this);

	cell count = array_capacity(alist.untagged());

	if(count == 0)
		return;

	cell i;
	for(i = 0; i < count; i++)
	{
		gc_root<array> pair(array_nth(alist.untagged(),i),this);

		gc_root<word> word(array_nth(pair.untagged(),0),this);
		gc_root<object> data(array_nth(pair.untagged(),1),this);

		switch(data.type())
		{
		case QUOTATION_TYPE:
			jit_compile_word(word.value(),data.value(),false);
			break;
		case ARRAY_TYPE:
			{
				array *compiled_data = data.as<array>().untagged();
				cell owner = array_nth(compiled_data,0);
				cell literals = array_nth(compiled_data,1);
				cell relocation = array_nth(compiled_data,2);
				cell labels = array_nth(compiled_data,3);
				cell code = array_nth(compiled_data,4);

				code_block *compiled = add_code_block(
					code_block_optimized,
					code,
					labels,
					owner,
					relocation,
					literals);

				word->code = compiled;
			}
			break;
		default:
			critical_error("Expected a quotation or an array",data.value());
			break;
		}

		update_word_xt(word.value());
	}

	update_code_heap_words();
}

/* Push the free space and total size of the code heap */
void factor_vm::primitive_code_room()
{
	cell used, total_free, max_free;
	code->allocator->usage(&used,&total_free,&max_free);
	dpush(tag_fixnum(code->seg->size / 1024));
	dpush(tag_fixnum(used / 1024));
	dpush(tag_fixnum(total_free / 1024));
	dpush(tag_fixnum(max_free / 1024));
}

code_block *code_heap::forward_code_block(code_block *compiled)
{
	return (code_block *)allocator->state.forward_block(compiled);
}

struct callframe_forwarder {
	factor_vm *parent;

	explicit callframe_forwarder(factor_vm *parent_) : parent(parent_) {}

	void operator()(stack_frame *frame)
	{
		cell offset = (cell)FRAME_RETURN_ADDRESS(frame,parent) - (cell)frame->xt;

		code_block *forwarded = parent->code->forward_code_block(parent->frame_code(frame));
		frame->xt = forwarded->xt();

		FRAME_RETURN_ADDRESS(frame,parent) = (void *)((cell)frame->xt + offset);
	}
};

void factor_vm::forward_object_xts()
{
	begin_scan();

	cell obj;

	while(to_boolean(obj = next_object()))
	{
		switch(tagged<object>(obj).type())
		{
		case WORD_TYPE:
			{
				word *w = untag<word>(obj);

				if(w->code)
					w->code = code->forward_code_block(w->code);
				if(w->profiling)
					w->profiling = code->forward_code_block(w->profiling);

				update_word_xt(obj);
			}
			break;
		case QUOTATION_TYPE:
			{
				quotation *quot = untag<quotation>(obj);

				if(quot->code)
				{
					quot->code = code->forward_code_block(quot->code);
					set_quot_xt(quot,quot->code);
				}
			}
			break;
		case CALLSTACK_TYPE:
			{
				callstack *stack = untag<callstack>(obj);
				callframe_forwarder forwarder(this);
				iterate_callstack_object(stack,forwarder);
			}
			break;
		default:
			break;
		}
	}

	end_scan();
}

void factor_vm::forward_context_xts()
{
	callframe_forwarder forwarder(this);
	iterate_active_frames(forwarder);
}

struct callback_forwarder {
	code_heap *code;
	callback_heap *callbacks;

	callback_forwarder(code_heap *code_, callback_heap *callbacks_) :
		code(code_), callbacks(callbacks_) {}

	void operator()(callback *stub)
	{
		stub->compiled = code->forward_code_block(stub->compiled);
		callbacks->update(stub);
	}
};

void factor_vm::forward_callback_xts()
{
	callback_forwarder forwarder(code,callbacks);
	callbacks->iterate(forwarder);
}

/* Move all free space to the end of the code heap. Live blocks must be marked
on entry to this function. XTs in code blocks must be updated after this
function returns. */
void factor_vm::compact_code_heap(bool trace_contexts_p)
{
	/* Figure out where blocks are going to go */
	code->allocator->state.compute_forwarding();

	/* Update references to the code heap from the data heap */
	forward_object_xts();
	if(trace_contexts_p)
	{
		forward_context_xts();
		forward_callback_xts();
	}

	/* Move code blocks and update references amongst them (this requires
	that the data heap is up to date since relocation looks up object XTs) */
	code_heap_relocator relocator(this);
	code_heap_iterator<code_heap_relocator> iter(relocator);
	code->allocator->compact(iter);
}

struct stack_trace_stripper {
	explicit stack_trace_stripper() {}

	void operator()(code_block *compiled, cell size)
	{
		compiled->owner = false_object;
	}
};

void factor_vm::primitive_strip_stack_traces()
{
	stack_trace_stripper stripper;
	iterate_code_heap(stripper);
}

}
