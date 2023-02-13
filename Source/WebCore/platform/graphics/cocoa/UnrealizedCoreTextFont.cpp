/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UnrealizedCoreTextFont.h"

#include "FontCreationContext.h"
#include "FontDescription.h"

#include <optional>

namespace WebCore {

static std::optional<CGFloat> getCGFloatValue(CFNumberRef number)
{
    ASSERT(number);
    ASSERT(CFGetTypeID(number) == CFNumberGetTypeID());
    CGFloat value = 0;
    auto success = CFNumberGetValue(number, kCFNumberCGFloatType, &value);
    if (success && value)
        return value;
    return std::nullopt;
}

CGFloat UnrealizedCoreTextFont::getSize() const
{
    if (auto sizeAttribute = static_cast<CFNumberRef>(CFDictionaryGetValue(m_attributes.get(), kCTFontSizeAttribute))) {
        if (auto result = getCGFloatValue(sizeAttribute))
            return *result;
    }

    auto sizeAttribute = WTF::switchOn(m_baseFont, [](const RetainPtr<CTFontRef>& font) {
        return adoptCF(static_cast<CFNumberRef>(CTFontCopyAttribute(font.get(), kCTFontSizeAttribute)));
    }, [](const RetainPtr<CTFontDescriptorRef>& fontDescriptor) {
        return adoptCF(static_cast<CFNumberRef>(CTFontDescriptorCopyAttribute(fontDescriptor.get(), kCTFontSizeAttribute)));
    });
    if (sizeAttribute) {
        if (auto result = getCGFloatValue(sizeAttribute.get()))
            return *result;
    }

    return 0;
}

RetainPtr<CTFontRef> UnrealizedCoreTextFont::realize() const
{
    return WTF::switchOn(m_baseFont, [this](const RetainPtr<CTFontRef>& font) -> RetainPtr<CTFontRef> {
        if (!font)
            return nullptr;
        if (!CFDictionaryGetCount(m_attributes.get()))
            return font;
        auto modification = adoptCF(CTFontDescriptorCreateWithAttributes(m_attributes.get()));
        return adoptCF(CTFontCreateCopyWithAttributes(font.get(), getSize(), nullptr, modification.get()));
    }, [this](const RetainPtr<CTFontDescriptorRef>& fontDescriptor) -> RetainPtr<CTFontRef> {
        if (!fontDescriptor)
            return nullptr;
        if (!CFDictionaryGetCount(m_attributes.get()))
            return adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), getSize(), nullptr));
        auto updatedFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor.get(), m_attributes.get()));
        return adoptCF(CTFontCreateWithFontDescriptor(updatedFontDescriptor.get(), getSize(), nullptr));
    });
}

UnrealizedCoreTextFont::operator bool() const
{
    return WTF::switchOn(m_baseFont, [](const RetainPtr<CTFontRef>& font) -> bool {
        return font;
    }, [](const RetainPtr<CTFontDescriptorRef>& fontDescriptor) -> bool {
        return fontDescriptor;
    });
}

static inline void appendTrueTypeFeature(CFMutableArrayRef features, int type, int selector)
{
    auto typeNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &type));
    auto selectorNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &selector));
    CFTypeRef featureKeys[] = { kCTFontFeatureTypeIdentifierKey, kCTFontFeatureSelectorIdentifierKey };
    CFTypeRef featureValues[] = { typeNumber.get(), selectorNumber.get() };
    auto feature = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, featureKeys, featureValues, std::size(featureKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFArrayAppendValue(features, feature.get());
}

static inline bool tagEquals(FontTag tag, const char comparison[4])
{
    return equalIgnoringASCIICase(tag.data(), comparison, 4);
}

static inline void appendOpenTypeFeature(CFMutableArrayRef features, const FontFeature& feature)
{
    auto featureKey = adoptCF(CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(feature.tag().data()), feature.tag().size() * sizeof(FontTag::value_type), kCFStringEncodingASCII, false));
    int rawFeatureValue = feature.value();
    auto featureValue = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rawFeatureValue));
    CFTypeRef featureDictionaryKeys[] = { kCTFontOpenTypeFeatureTag, kCTFontOpenTypeFeatureValue };
    CFTypeRef featureDictionaryValues[] = { featureKey.get(), featureValue.get() };
    auto featureDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, featureDictionaryKeys, featureDictionaryValues, std::size(featureDictionaryValues), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFArrayAppendValue(features, featureDictionary.get());
}

static void addLightPalette(CFMutableDictionaryRef attributes)
{
    CFIndex light = kCTFontPaletteLight;
    auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &light));
    CFDictionarySetValue(attributes, kCTFontPaletteAttribute, number.get());
}

static void addDarkPalette(CFMutableDictionaryRef attributes)
{
    CFIndex dark = kCTFontPaletteDark;
    auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &dark));
    CFDictionarySetValue(attributes, kCTFontPaletteAttribute, number.get());
}

static void addAttributesForCustomFontPalettes(CFMutableDictionaryRef attributes, std::optional<FontPaletteIndex> basePalette, const Vector<FontPaletteValues::OverriddenColor>& overrideColors)
{
    if (basePalette) {
        switch (basePalette->type) {
        case FontPaletteIndex::Type::Light:
            addLightPalette(attributes);
            break;
        case FontPaletteIndex::Type::Dark:
            addDarkPalette(attributes);
            break;
        case FontPaletteIndex::Type::Integer: {
            int64_t rawIndex = basePalette->integer; // There is no kCFNumberUIntType.
            auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &rawIndex));
            CFDictionarySetValue(attributes, kCTFontPaletteAttribute, number.get());
            break;
        }
        }
    }

    if (!overrideColors.isEmpty()) {
        auto overrideDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        for (const auto& pair : overrideColors) {
            const auto& color = pair.second;
            int64_t rawIndex = pair.first; // There is no kCFNumberUIntType.
            auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &rawIndex));
            auto colorObject = cachedCGColor(color);
            CFDictionarySetValue(overrideDictionary.get(), number.get(), colorObject.get());
        }
        if (CFDictionaryGetCount(overrideDictionary.get()))
            CFDictionarySetValue(attributes, kCTFontPaletteColorsAttribute, overrideDictionary.get());
    }
}

static void addAttributesForFontPalettes(CFMutableDictionaryRef attributes, const FontPalette& fontPalette, const FontPaletteValues* fontPaletteValues)
{
    switch (fontPalette.type) {
    case FontPalette::Type::Normal:
        break;
    case FontPalette::Type::Light:
        addLightPalette(attributes);
        break;
    case FontPalette::Type::Dark:
        addDarkPalette(attributes);
        break;
    case FontPalette::Type::Custom: {
        if (fontPaletteValues)
            addAttributesForCustomFontPalettes(attributes, fontPaletteValues->basePalette(), fontPaletteValues->overrideColors());
        break;
    }
    }
}

static void modifyFromContext(CFMutableDictionaryRef attributes, const FontDescription& fontDescription, const FontCreationContext& fontCreationContext, bool applyWeightWidthSlopeVariations)
{
    auto fontOpticalSizing = fontDescription.opticalSizing();
    auto fontSelectionRequest = fontDescription.fontSelectionRequest();
    auto fontStyleAxis = fontDescription.fontStyleAxis();
    const auto& variations = fontDescription.variationSettings();
    const auto& features = fontDescription.featureSettings();
    const auto& variantSettings = fontDescription.variantSettings();
    auto textRenderingMode = fontDescription.textRenderingMode();
    auto shouldDisableLigaturesForSpacing = fontDescription.shouldDisableLigaturesForSpacing();

    // This algorithm is described at https://drafts.csswg.org/css-fonts-4/#feature-variation-precedence
    FeaturesMap featuresToBeApplied;
    using VariationsMap = HashMap<FontTag, float, FourCharacterTagHash, FourCharacterTagHashTraits>;
    VariationsMap variationsToBeApplied;

    auto applyFeature = [&](const FontTag& tag, int value) {
        // AAT doesn't differentiate between liga and clig. We need to make sure they always agree.
        featuresToBeApplied.set(tag, value);
        if (tag == fontFeatureTag("liga"))
            featuresToBeApplied.set(fontFeatureTag("clig"), value);
        else if (tag == fontFeatureTag("clig"))
            featuresToBeApplied.set(fontFeatureTag("liga"), value);
    };

    // Step 1: CoreText handles default features (such as required ligatures).

    // Step 2: font-weight, font-stretch, and font-style
    // The system font is somewhat magical. Don't mess with its variations.
    if (applyWeightWidthSlopeVariations
#if USE(NON_VARIABLE_SYSTEM_FONT)
        && !fontIsSystemFont(originalFont)
#endif
    ) {
        float weight = fontSelectionRequest.weight;
        float width = fontSelectionRequest.width;
        float slope = fontSelectionRequest.slope.value_or(normalItalicValue());
        if (auto weightValue = fontCreationContext.fontFaceCapabilities().weight)
            weight = std::max(std::min(weight, static_cast<float>(weightValue->maximum)), static_cast<float>(weightValue->minimum));
        if (auto widthValue = fontCreationContext.fontFaceCapabilities().width)
            width = std::max(std::min(width, static_cast<float>(widthValue->maximum)), static_cast<float>(widthValue->minimum));
        if (auto slopeValue = fontCreationContext.fontFaceCapabilities().weight)
            slope = std::max(std::min(slope, static_cast<float>(slopeValue->maximum)), static_cast<float>(slopeValue->minimum));
        variationsToBeApplied.set({{'w', 'g', 'h', 't'}}, weight);
        variationsToBeApplied.set({{'w', 'd', 't', 'h'}}, width);
        if (fontStyleAxis == FontStyleAxis::ital)
            variationsToBeApplied.set({{'i', 't', 'a', 'l'}}, 1);
        else
            variationsToBeApplied.set({{'s', 'l', 'n', 't'}}, slope);
    }

    // FIXME: Implement Step 5: font-named-instance

    // FIXME: Implement Step 6: the font-variation-settings descriptor inside @font-face

    // Step 7: Consult with font-feature-settings inside @font-face
    if (fontCreationContext.fontFaceFeatures()) {
        for (auto& fontFaceFeature : *fontCreationContext.fontFaceFeatures())
            applyFeature(fontFaceFeature.tag(), fontFaceFeature.value());
    }

    // FIXME: Move font-optical-sizing handling here. It should be step 9.

    // Step 10: Font-variant
    for (auto& newFeature : computeFeatureSettingsFromVariants(variantSettings, fontCreationContext.fontFeatureValues()))
        applyFeature(newFeature.key, newFeature.value);

    // Step 11: Other properties
    if (textRenderingMode == TextRenderingMode::OptimizeSpeed) {
        applyFeature(fontFeatureTag("liga"), 0);
        applyFeature(fontFeatureTag("clig"), 0);
        applyFeature(fontFeatureTag("dlig"), 0);
        applyFeature(fontFeatureTag("hlig"), 0);
        applyFeature(fontFeatureTag("calt"), 0);
    }
    if (shouldDisableLigaturesForSpacing) {
        applyFeature(fontFeatureTag("liga"), 0);
        applyFeature(fontFeatureTag("clig"), 0);
        applyFeature(fontFeatureTag("dlig"), 0);
        applyFeature(fontFeatureTag("hlig"), 0);
        // Core Text doesn't disable calt when letter-spacing is applied, so we won't either.
    }

    // Step 13: Font-feature-settings
    for (auto& newFeature : features)
        applyFeature(newFeature.tag(), newFeature.value());

    // Step 12: font-variation-settings
    for (auto& newVariation : variations)
        variationsToBeApplied.set(newVariation.tag(), newVariation.value());

    if (!featuresToBeApplied.isEmpty()) {
        RetainPtr<CFMutableArrayRef> featureArray;
        if (auto fontFeatureSettings = static_cast<CFArrayRef>(CFDictionaryGetValue(attributes, kCTFontFeatureSettingsAttribute)))
            featureArray = adoptCF(CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, fontFeatureSettings));
        else
            featureArray = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, features.size(), &kCFTypeArrayCallBacks));

        for (auto& p : featuresToBeApplied) {
            auto feature = FontFeature(p.key, p.value);

            // CoreText does not map hlig and hist for TrueType fonts.
            if (tagEquals(feature.tag(), "hlig") || tagEquals(feature.tag(), "hist")) {
                if (feature.enabled())
                    appendTrueTypeFeature(featureArray.get(), kLigaturesType, kHistoricalLigaturesOnSelector);
                else if (tagEquals(feature.tag(), "hlig"))
                    appendTrueTypeFeature(featureArray.get(), kLigaturesType, kHistoricalLigaturesOffSelector);
                continue;
            }

            appendOpenTypeFeature(featureArray.get(), feature);
        }

        CFDictionarySetValue(attributes, kCTFontFeatureSettingsAttribute, featureArray.get());
    }
    if (!variationsToBeApplied.isEmpty()) {
        RetainPtr<CFMutableDictionaryRef> variationDictionary;
        if (auto fontVariations = static_cast<CFDictionaryRef>(CFDictionaryGetValue(attributes, kCTFontVariationAttribute)))
            variationDictionary = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, fontVariations));
        else
            variationDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

        for (auto& p : variationsToBeApplied) {
            long long bitwiseTag = p.key[0] << 24 | p.key[1] << 16 | p.key[2] << 8 | p.key[3];
            auto tagNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &bitwiseTag));
            auto valueNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &p.value));
            CFDictionarySetValue(variationDictionary.get(), tagNumber.get(), valueNumber.get());
        }

        CFDictionarySetValue(attributes, kCTFontVariationAttribute, variationDictionary.get());
    }

    // Step 9: font-optical-sizing
    // FIXME: Apply this before font-variation-settings
    if (textRenderingMode == TextRenderingMode::OptimizeLegibility)
        CFDictionarySetValue(attributes, kCTFontOpticalSizeAttribute, CFSTR("auto"));
    else if (fontOpticalSizing == FontOpticalSizing::Disabled)
        CFDictionarySetValue(attributes, kCTFontOpticalSizeAttribute, CFSTR("none"));

    addAttributesForFontPalettes(attributes, fontDescription.fontPalette(), fontCreationContext.fontPaletteValues());

    addAttributesForInstalledFonts(attributes, fontDescription.shouldAllowUserInstalledFonts());
}

void UnrealizedCoreTextFont::modifyFromContext(const FontDescription& fontDescription, const FontCreationContext& fontCreationContext, bool applyWeightWidthSlopeVariations)
{
    modify([&](CFMutableDictionaryRef attributes) {
        WebCore::modifyFromContext(attributes, fontDescription, fontCreationContext, applyWeightWidthSlopeVariations);
    });
}

} // namespace WebCore
