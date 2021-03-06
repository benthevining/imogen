
#include <imogen_gui/imogen_gui.h>
#include <bv_app_utils/bv_app_utils.h>

namespace Imogen
{
struct RemoteApp : bav::GuiApp< Remote >
{
    RemoteApp()
        : bav::GuiApp< Imogen::Remote > (String ("Imogen ") + TRANS ("Remote"), "0.0.1")
    {
    }
};

}  // namespace Imogen

START_JUCE_APPLICATION (Imogen::RemoteApp)
