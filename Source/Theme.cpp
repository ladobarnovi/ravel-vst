#include "Theme.h"

#include <BinaryData.h>

namespace theme
{
    namespace
    {
        /** The two faces, created once and kept.

            juce::Typeface::createSystemTypefaceFor() parses the whole font file, and every
            string in this UI is measured before it is drawn -- a settings column's width, a
            chip's width, where a value's read-out starts. Rebuilding the typeface for each of
            those would parse 120KB of TrueType per measurement.

            Function-local statics rather than namespace-scope ones so they are constructed on
            first use. A namespace-scope Typeface::Ptr would be built during static
            initialisation, which in a plugin runs while the host is loading the binary and
            before JUCE's own graphics stack has been brought up.
        */
        juce::Typeface::Ptr regularTypeface()
        {
            static const juce::Typeface::Ptr face = juce::Typeface::createSystemTypefaceFor (
                BinaryData::ArchivoRegular_ttf, BinaryData::ArchivoRegular_ttfSize);
            return face;
        }

        juce::Typeface::Ptr semiBoldTypeface()
        {
            static const juce::Typeface::Ptr face = juce::Typeface::createSystemTypefaceFor (
                BinaryData::ArchivoSemiBold_ttf, BinaryData::ArchivoSemiBold_ttfSize);
            return face;
        }
    }

    juce::Font regularFont (float height)
    {
        return juce::Font (juce::FontOptions (regularTypeface()).withHeight (height));
    }

    juce::Font semiBoldFont (float height)
    {
        // A real SemiBold face rather than juce::Font::bold applied to the regular one, which
        // synthesises weight by smearing the glyphs sideways -- at 11px that reads as blur
        // rather than as weight, and it also widens every string, so a heading measured one
        // way and drawn the other would overflow its column.
        return juce::Font (juce::FontOptions (semiBoldTypeface()).withHeight (height));
    }
}
