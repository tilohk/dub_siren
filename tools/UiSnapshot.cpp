// Renders the plug-in editor to PNG files without opening a window:
//   SirenUiSnapshot <output-dir>
// Writes idle.png and active.png (trigger held, freeze on).
#include "PluginProcessor.h"
#include "params/Parameters.h"

#include <juce_audio_utils/juce_audio_utils.h>

static void snapshot (DubSirenProcessor& processor, const juce::File& file)
{
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::cout << "wrote " << file.getFullPathName() << std::endl;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                   : juce::File::getCurrentWorkingDirectory());
    dir.createDirectory();

    DubSirenProcessor processor;
    snapshot (processor, dir.getChildFile ("idle.png"));

    processor.gateActive = true;
    processor.apvts.getParameter (params::id::echoFreeze)->setValueNotifyingHost (1.0f);
    processor.apvts.getParameter (params::id::lfoShape)->setValueNotifyingHost (1.0f); // Rotor: Accel greyed out
    snapshot (processor, dir.getChildFile ("active.png"));
    return 0;
}
