! Copyright (C) 2005, 2009 Slava Pestov.
! Copyright (C) 2006, 2007 Alex Chapman.
! See http://factorcode.org/license.txt for BSD license.
USING: arrays kernel sequences ui.gadgets ui.pens.solid
ui.pens.gradient ui.text ui.images colors colors.gray
colors.constants accessors io.pathnames ;
QUALIFIED: colors
IN: ui.gadgets.theme

: theme-image ( name -- image-name )
    "resource:basis/ui/gadgets/theme/" prepend-path ".tiff" append <image-name> ;

: solid-interior ( gadget color -- gadget )
    <solid> >>interior ; inline

: solid-boundary ( gadget color -- gadget )
    <solid> >>boundary ; inline

: faint-boundary ( gadget -- gadget )
    COLOR: gray solid-boundary ; inline

: selection-color ( -- color ) T{ rgba f 0.8 0.8 1.0 1.0 } ; inline

: focus-border-color ( -- color ) COLOR: dark-gray ; inline

: plain-gradient ( -- gradient )
    {
        T{ gray f 0.94 1.0 }
        T{ gray f 0.83 1.0 }
        T{ gray f 0.83 1.0 }
        T{ gray f 0.62 1.0 }
    } <gradient> ;

: rollover-gradient ( -- gradient )
    {
        T{ gray f 1.0  1.0 }
        T{ gray f 0.9  1.0 }
        T{ gray f 0.9  1.0 }
        T{ gray f 0.75 1.0 }
    } <gradient> ;

: pressed-gradient ( -- gradient )
    {
        T{ gray f 0.75 1.0 }
        T{ gray f 0.9  1.0 }
        T{ gray f 0.9  1.0 }
        T{ gray f 1.0  1.0 }
    } <gradient> ;

: selected-gradient ( -- gradient )
    {
        T{ gray f 0.65 1.0 }
        T{ gray f 0.8  1.0 }
        T{ gray f 0.8  1.0 }
        T{ gray f 1.0  1.0 }
    } <gradient> ;

: lowered-gradient ( -- gradient )
    {
        T{ gray f 0.37 1.0 }
        T{ gray f 0.43 1.0 }
        T{ gray f 0.5  1.0 }
    } <gradient> ;
