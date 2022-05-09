/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleEqAudioProcessor::SimpleEqAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

SimpleEqAudioProcessor::~SimpleEqAudioProcessor()
{
}

//==============================================================================
const juce::String SimpleEqAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleEqAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEqAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEqAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleEqAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleEqAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleEqAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleEqAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleEqAudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleEqAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleEqAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    // Prepare our left & right ProcessorChain
    juce::dsp::ProcessSpec specs;

    // Maximum number of sample processed at once
    specs.maximumBlockSize = samplesPerBlock;

    // Number of channels, monoChains can only handle 1 channel at a time
    specs.numChannels = 1;

    // Sample rate
    specs.sampleRate = sampleRate;

    // Set the specs to each channel
    this->leftChain.prepare(specs);
    this->rightChain.prepare(specs);
}

void SimpleEqAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleEqAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SimpleEqAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This wraps the audio buffer
    juce::dsp::AudioBlock<float> block(buffer);

    // Get audio blocks
    // Extracting the left channel
    auto leftBlock = block.getSingleChannelBlock(0);
    // Extracting the left channel
    auto rightBlock = block.getSingleChannelBlock(1);

    // Create processing context to wrap audio blocks
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    // Pass contexts to ProcessChain
    this->leftChain.process(leftContext);
    this->rightChain.process(rightContext);


}

//==============================================================================
bool SimpleEqAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleEqAudioProcessor::createEditor()
{
    return new SimpleEqAudioProcessorEditor (*this);
}

//==============================================================================
void SimpleEqAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SimpleEqAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleEqAudioProcessor();
}






/*
  ==============================================================================

    The below code is independant from the Juce plugin Processor.

  ==============================================================================
*/

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    // Collection of parameters
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // String collection of different slopes choices for the frequency cut
    juce::StringArray cutSlopes;

    for (int i = 0, i < 4; i++) {
        juce::String newSlope;
        newSlope << (12 + (i * 12));
        newSlope << " dB/Oct";
        cutSlopes.add(newSlope);
    }

    // Frequency range (range start freq, Range end freq, Range step, Range skew step)
    // Skew range determines if the response is linear or not
    juce::NormalisableRange<float> frequencyRange(20.f, 20000.f, 1.f, 1.f);

    // Gain range (range start gain, Range end gain, Range step, Range skew step)
    // -24dB, 24dB, 0.5 dB as step, 0.5dB step is uniform
    juce::NormalisableRange<float> gainRange(-24.f, 24.f, 0.5f, 1.f);

    // Peak tightness/wideness range (range start gain, Range end gain, Range step, Range skew step)
    juce::NormalisableRange<float> tightnessRange(0.1f, 10.f, 0.05f, 1.f);

    // Adding a LowCut Freq parameter of type float to the ParameterLayout
    // (Param ID, Param name, juce::NormalisableRange<float>, Param default value)
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Frequency",
        "LowCut Frequency",
        frequencyRange,
        20.f);
    )

    // Adding a HighCut Freq parameter of type float to the ParameterLayout
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Frequency",
        "HighCut Frequency",
        frequencyRange,
        20000.f);
    )

    // Adding a Peak Freq parameter of type float to the ParameterLayout
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak (1) Frequency",
        "Peak (1) Frequency",
        frequencyRange,
        1000.f);

    // Adding a Peak Gain parameter of type float to the ParameterLayout
    // This parameter will control the effective gain of the Peak frequency
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak (1) Gain",
        "Peak (1) Gain",
        gainRange,
        0.f);

    // Adding a Peak tightness parameter of type float to the ParameterLayout
    // This parameter will define the tightness/wideness of the frequency range
    // the Peak Fresquency applies too.
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak (1) Tightness",
        "Peak (1) Tightness",
        gainRange,
        1.f);

    // Adding a frequency cut slope type parameter of type choice
    // These parameter will select a given slope for the frequency cut,
    // i.e. progressive, steep, brickwall...
    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope",
        "LowCut Slope",
        cutSlopes,
        0);

    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope",
        "HighCut Slope",
        cutSlopes,
        0);

    return (layout);
}