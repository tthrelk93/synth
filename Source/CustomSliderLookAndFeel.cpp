//
//  CustomSliderLookAndFeel.cpp
//  MiniMoog - Shared Code
//
//  Created by Thomas Threlkeld on 11/24/23.
//

#include <stdio.h>
#include "CustomSliderLookAndFeel.h"

void CustomSliderLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                               int x, int y, int width, int height,
                                               float sliderPosProportional,
                                               float rotaryStartAngle,
                                               float rotaryEndAngle,
                                               juce::Slider& slider) {
    auto radius = juce::jmin(width / 2, height / 2) - 4.0f;
    auto centreX = x + width * 0.5f;
    auto centreY = y + height * 0.5f;
    juce::Colour backgroundColour ;
    // Set the background color with the interpolated alpha
    if(sliderKey == "noiseVolKnob"){
        backgroundColour = juce::Colour::fromHSV(0, 0, 0, 0.2); // Black with interpolated alpha
    } else {
        backgroundColour = juce::Colour::fromHSV(0, 0, 0, sliderPosProportional); // Black with interpolated alpha
    }
    
    // Draw the base circle with the background color
    g.setColour(backgroundColour);
    g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
    bool isNegativeKnobVal = false;
    // Number of iris blades
    int numBlades = 0;
    float numBladesFloat = 0.0;
    if(sliderKey == "osc1RangeKnob" || sliderKey == "osc2RangeKnob" || sliderKey == "osc3RangeKnob"){
        const float epsilon = 0.0001f; // A small tolerance value

        if (std::abs(sliderPosProportional - 0.0f) < epsilon) {
            numBlades = 64;
        } else if (std::abs(sliderPosProportional - 0.2f) < epsilon) {
            numBlades = 32;
        } else if (std::abs(sliderPosProportional - 0.4f) < epsilon) {
            numBlades = 16;
        } else if (std::abs(sliderPosProportional - 0.6f) < epsilon) {
            numBlades = 8;
        } else if (std::abs(sliderPosProportional - 0.8f) < epsilon) {
            numBlades = 4;
        } else {
            numBlades = 2;
        }

       
    } else if( sliderKey == "osc2FreqKnob" || sliderKey == "osc3FreqKnob"){
        
        const float epsilon = 0.0001f; // A small tolerance value

        if (std::abs(sliderPosProportional - 0.0f) < epsilon) {
            isNegativeKnobVal = true;
            numBlades = 8;
        } else if (std::abs(sliderPosProportional - 0.0625f) < epsilon) {
            isNegativeKnobVal = true;
            numBlades = 7;
        } else if (std::abs(sliderPosProportional - 0.125f) < epsilon) {
            isNegativeKnobVal = true;
            numBlades = 6;
        } else if (std::abs(sliderPosProportional - 0.1875f) < epsilon) {
            isNegativeKnobVal = true;
            numBlades = 5;
        } else if (std::abs(sliderPosProportional - 0.25f) < epsilon) {
            isNegativeKnobVal = true;
            numBlades = 4;
        } else if (std::abs(sliderPosProportional - 0.3125f) < epsilon) {
            isNegativeKnobVal = true;
            numBlades = 3;
        } else if (std::abs(sliderPosProportional - 0.375f) < epsilon) {
            isNegativeKnobVal = true;
            numBlades = 2;
        } else if (std::abs(sliderPosProportional - 0.4375f) < epsilon) {
            isNegativeKnobVal = true;
            numBlades = 1;
        } else if (std::abs(sliderPosProportional - 0.5f) < epsilon) {
            numBlades = 0;
        } else if (std::abs(sliderPosProportional - 0.5625f) < epsilon) {
            numBlades = 1;
        } else if (std::abs(sliderPosProportional - 0.625f) < epsilon) {
            numBlades = 2;
        } else if (std::abs(sliderPosProportional - 0.6875f) < epsilon) {
            numBlades = 3;
        } else if (std::abs(sliderPosProportional - 0.75f) < epsilon) {
            numBlades = 4;
        } else if (std::abs(sliderPosProportional - 0.8125f) < epsilon) {
            numBlades = 4;
        } else if (std::abs(sliderPosProportional - 0.875f) < epsilon) {
            numBlades = 5;
        } else if (std::abs(sliderPosProportional - 0.9375f) < epsilon) {
            numBlades = 6;
        } else {
            numBlades = 7;
        }
    } else if( sliderKey == "ctrlTuneKnob" ){
       // juce::Logger::writeToLog("pos: " + juce::String(sliderPosProportional));
        const float epsilon = 0.0001f; // A small tolerance value

        if (std::abs(sliderPosProportional - 0.0f) < epsilon) {
            isNegativeKnobVal = true;
            numBladesFloat = 2.5;
        } else if (std::abs(sliderPosProportional - 0.1f) < epsilon) {
            isNegativeKnobVal = true;
            numBladesFloat = 2.0;
        } else if (std::abs(sliderPosProportional - 0.2f) < epsilon) {
            isNegativeKnobVal = true;
            numBladesFloat = 1.5;
        } else if (std::abs(sliderPosProportional - 0.3f) < epsilon) {
            isNegativeKnobVal = true;
            numBladesFloat = 1.0;
        } else if (std::abs(sliderPosProportional - 0.4f) < epsilon) {
            isNegativeKnobVal = true;
            numBladesFloat = 0.5;
        } else if (std::abs(sliderPosProportional - 0.5f) < epsilon) {
            numBladesFloat = 0.0;
        } else if (std::abs(sliderPosProportional - 0.6f) < epsilon) {
            numBladesFloat = 0.5;
        } else if (std::abs(sliderPosProportional - 0.7f) < epsilon) {
            numBladesFloat = 1.0;
        } else if (std::abs(sliderPosProportional - 0.8f) < epsilon) {
            numBladesFloat = 1.5;
        } else if (std::abs(sliderPosProportional - 0.9f) < epsilon) {
            numBladesFloat = 2.0;
        } else {
            numBladesFloat = 2.5;
        }
      
    } else if( sliderKey == "noiseVolKnob" ){
        numBlades = 0;
    } else {
        numBlades = sliderPosProportional * 10;
    }
    // Draw the outer border of the slider

        juce::Path borderPath;
        borderPath.addEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
    // Adjust the handle angle to match the slider position
    float handleAngle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * sliderPosProportional;
    
    // Correct the handle angle to point accurately
    handleAngle -= juce::MathConstants<float>::pi / 2.0f; // Adjust by 90 degrees
    
    // Adjust handle position to be just outside the slider's edge
    const float handleLength = 0.5f; // Length of the sides of the triangle
    const float handleDistanceFromCentre = radius + handleLength / 2; // Position just outside the slider's edge
    
    // Calculate the points for the triangle handle
    juce::Path trianglePath;
    
    trianglePath.addTriangle(centreX + handleDistanceFromCentre * std::cos(handleAngle),
                             centreY + handleDistanceFromCentre * std::sin(handleAngle),
                             centreX + (handleDistanceFromCentre - handleLength) * std::cos(handleAngle + juce::MathConstants<float>::pi / 9),
                             centreY + (handleDistanceFromCentre - handleLength) * std::sin(handleAngle + juce::MathConstants<float>::pi / 9),
                             centreX + (handleDistanceFromCentre - handleLength) * std::cos(handleAngle - juce::MathConstants<float>::pi / 9),
                             centreY + (handleDistanceFromCentre - handleLength) * std::sin(handleAngle - juce::MathConstants<float>::pi / 9));
    
    g.setColour(juce::Colours::whitesmoke); // Color of the handle
    g.fillPath(trianglePath); // Fill the triangle
   
    
    if (isOscWaveformSlider) {
        float alpha = 0.7;
       
        g.setColour(juce::Colours::papayawhip.withAlpha(alpha)); // Border color
        
        g.strokePath(borderPath, juce::PathStrokeType(0.5f)); // Border thickness
            // Draw waveform in the center based on slider position
            drawWaveform(g, slider.getValue(), x, y, width, height);
    } else {
        float alpha = sliderPosProportional;
        if(sliderKey == "noiseVolKnob"){
            alpha = 0.4;
        } else {
            if(alpha < 0.5){
                alpha = 0.7;
            }
        }
        juce::Colour lineColour;
        if(isNegativeKnobVal){
            lineColour = juce::Colours::maroon.withAlpha(alpha); // Border color
        } else {
            lineColour = juce::Colours::papayawhip.withAlpha(alpha); // Border color
        }
        g.setColour(lineColour); // Border color
        g.strokePath(borderPath, juce::PathStrokeType(0.5f)); // Border thickness
    // Maximum rotation angle for the blades
    const float maxRotationAngle = juce::MathConstants<float>::pi / 4.0f; // 45 degrees
    
    // Small angle offset to prevent overlap
    const float angleOffset = 5.0f; // Small angle to reduce overlap
    
        if(sliderKey == "ctrlTuneKnob"){
            // Draw the iris blades with borders
            for (float i = 0.0; i < numBladesFloat; i = i + 0.5) {
                float baseAngle = rotaryStartAngle + (float(i) / numBladesFloat) * 2.0f * juce::MathConstants<float>::pi;
                float rotation = maxRotationAngle * sliderPosProportional; // Rotate based on slider position
                float bladeAngle = baseAngle + rotation;
                
                float innerRadius = radius * sliderPosProportional; // Inner radius based on slider position
                
                
                juce::Path bladePath;
                bladePath.addPieSegment(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f,
                                        bladeAngle - maxRotationAngle + angleOffset,
                                        bladeAngle + maxRotationAngle - angleOffset,
                                        innerRadius / radius);
                
                float alpha = sliderPosProportional;
                if(alpha < 0.5){
                    alpha = 0.8;
                }
                juce::Colour pieColor;
                if(isNegativeKnobVal){
                    pieColor = juce::Colours::maroon.withAlpha(alpha); // Border color
                } else {
                    pieColor = juce::Colours::papayawhip.withAlpha(alpha); // Border color
                }
                g.setColour(pieColor); // Border color
                g.strokePath(bladePath, juce::PathStrokeType(0.5f)); // Border thickness
            }
        } else {
            // Draw the iris blades with borders
            for (int i = 0; i < numBlades; ++i) {
                float baseAngle = rotaryStartAngle + (float(i) / numBlades) * 2.0f * juce::MathConstants<float>::pi;
                float rotation = maxRotationAngle * sliderPosProportional; // Rotate based on slider position
                float bladeAngle = baseAngle + rotation;
                
                float innerRadius = radius * sliderPosProportional; // Inner radius based on slider position
                
                
                juce::Path bladePath;
                bladePath.addPieSegment(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f,
                                        bladeAngle - maxRotationAngle + angleOffset,
                                        bladeAngle + maxRotationAngle - angleOffset,
                                        innerRadius / radius);
                
                float alpha = sliderPosProportional;
                if(sliderKey == "noiseVolKnob"){
                    alpha = 0.4;
                } else {
                    if(alpha < 0.5){
                        alpha = 0.8;
                    }
                }
                juce::Colour pieColor;
                if(isNegativeKnobVal){
                    pieColor = juce::Colours::maroon.withAlpha(alpha); // Border color
                } else {
                    pieColor = juce::Colours::papayawhip.withAlpha(alpha); // Border color
                }
                g.setColour(pieColor); // Border color
                g.strokePath(bladePath, juce::PathStrokeType(0.5f)); // Border thickness
            }
        }
        
        auto textBounds = juce::Rectangle<float>(x, y, width, height).reduced(10); // Adjust as needed
        g.setColour(juce::Colours::white); // Text color
        auto text = juce::String(slider.getValue(), 2); // 2 decimal places

        g.drawText(createTextLabelForSlider(slider.getValue()), textBounds, juce::Justification::centred, false);
        
      
    }
}

juce::String CustomSliderLookAndFeel::createTextLabelForSlider(float i){
    int index = int(i);
    if (sliderKey == "osc1RangeKnob" || sliderKey == "osc2RangeKnob" || sliderKey == "osc3RangeKnob" ) {
        
        switch(index){
            case 0:
                return "LO";
            case 1:
                return "32'";
            case 2:
                return "16'";
            case 3:
                return "8'";
            case 4:
                return "4'";
            default:
                return "2'";
        }
    } else if(sliderKey == "ctrlTuneKnob") {
        
        return juce::String(i);
       
    } else if(sliderKey == "ctrlModMixKnob") {
        switch(index){
            case 0:
                return "Osc 3";
            case 1:
                return "1";
            case 2:
                return "2";
            case 3:
                return "3";
            case 4:
                return "4";
            case 5:
                return "5";
            case 6:
                return "6";
            case 7:
                return "7";
            case 8:
                return "8";
            case 9:
                return "9";
            default:
                return "Noise";
        }
    } else {
            // Draw the text box in the center
            return juce::String(index);
    }
    
}

void CustomSliderLookAndFeel::drawWaveform(juce::Graphics& g, int index, int x, int y, int width, int height) {
    auto center = juce::Point<float>(x + width / 2, y + height / 2);
    auto radius = juce::jmin(width / 2, height / 2) - 20; // Margin

    g.setColour(juce::Colours::white);

    juce::Path waveform;
    //juce::Logger::writeToLog("index: " + juce::String(index));
    bool highSegment = true;
    switch (index) {
        case 0: // Triangle Waveform
            waveform.startNewSubPath(center.x - radius, center.y);
            waveform.lineTo(center.x, center.y - radius);
            waveform.lineTo(center.x + radius, center.y);
            break;
        case 1: // Sharktooth Waveform
            waveform.startNewSubPath(center.x - radius, center.y);

               // First segment - rising to the middle peak
               waveform.lineTo(center.x, center.y - radius);

               // Middle peak - sharp jagged point like a saw
              
               waveform.lineTo(center.x + (radius / 4.0f) / 2, center.y - radius / 2); // Halfway down the peak
               waveform.lineTo(center.x, center.y - radius); // Back to the top
               waveform.lineTo(center.x + (radius / 4.0f) / 2, center.y - radius / 2); // Halfway down the peak again

               // Second segment - falling to the baseline from the low point of the sharktooth
               waveform.lineTo(center.x + radius, center.y);
            
         
            break;
        case 2: // ReverseSaw Waveform
            waveform.startNewSubPath(center.x - radius, center.y + (radius * 0.5f)); // Start at a lower amplitude
            for (float i = -radius; i <= radius; i += (radius / 1.0f)) { // Draw fewer peaks
                if (i < radius) {
                    waveform.lineTo(center.x + i, center.y - (radius * 0.5f)); // Half the original amplitude
                    waveform.lineTo(center.x + i + (radius / 1.0f), center.y + (radius * 0.5f)); // Half the original amplitude
                } else {
                    waveform.lineTo(center.x + i, center.y - (radius * 0.5f));
                }
            }
            break;
        case 3: // Sawtooth Waveform
            waveform.startNewSubPath(center.x - radius, center.y - (radius * 0.5f)); // Start at a lower amplitude
            for (float i = -radius; i <= radius; i += (radius / 1.0f)) { // Draw fewer peaks
                if (i < radius) {
                    waveform.lineTo(center.x + i, center.y + (radius * 0.5f)); // Half the original amplitude
                    waveform.lineTo(center.x + i + (radius / 1.0f), center.y - (radius * 0.5f)); // Half the original amplitude
                } else {
                    waveform.lineTo(center.x + i, center.y + (radius * 0.5f));
                }
            }
            break;
        case 4: // Square Waveform
           
              

               waveform.startNewSubPath(center.x - radius, center.y + (radius * 0.6f)); // Start at bottom left
               waveform.lineTo(center.x - radius, center.y - (radius * 0.6f)); // Line up to top left
               waveform.lineTo(center.x + radius, center.y - (radius * 0.6f)); // Line across to top right
               waveform.lineTo(center.x + radius, center.y + (radius * 0.6f)); // Line down to bottom right
           
            break;


        case 5: // WideRectangle Waveform
            waveform.startNewSubPath(center.x - radius, center.y - radius / 2);
            waveform.lineTo(center.x - radius, center.y + radius / 2);
            waveform.lineTo(center.x + radius, center.y + radius / 2);
            waveform.lineTo(center.x + radius, center.y - radius / 2);
            break;
        case 6: // NarrowRectangle Waveform
            waveform.startNewSubPath(center.x - radius, center.y - radius / 4);
            waveform.lineTo(center.x - radius, center.y + radius / 4);
            waveform.lineTo(center.x + radius, center.y + radius / 4);
            waveform.lineTo(center.x + radius, center.y - radius / 4);
            break;
        default:
            break;
    }

    g.strokePath(waveform, juce::PathStrokeType(1.8f));
}








