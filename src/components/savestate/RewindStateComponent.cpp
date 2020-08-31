#include "RewindStateComponent.h"
#include "../../services/MultiEventHooker.h"
#include "../../utils/ImGuiExtensions.h"

RewindStateComponent::RewindStateComponent(BakkesMod::Plugin::BakkesModPlugin *plugin)
        : PluginComponent(plugin), previousSaveTime(), rewindBuffer(0.0f)
{

}

void RewindStateComponent::onLoad()
{
    // Function TAGame.Mutator_Freeplay_TA.Init
    // Function TAGame.GameEvent_TA.Init
    // Function TAGame.GameEvent_TA.Destroyed
    // Function TAGame.GameEvent_TA.EventDestroyed
    // Function TAGame.GameEvent_TA.IsFinished
    // Function GameEvent_Soccar_TA.Active.EndRound
    // Function TAGame.GameEvent_Soccar_TA.EventMatchEnded
    CVarWrapper componentEnabled = this->plugin->cvarManager->registerCvar("st_savestate_rewind_enabled", "1", "Is rewind state enabled");
    componentEnabled.addOnValueChanged([this](const std::string &oldValue, const CVarWrapper &cvar) {
        this->onComponentEnabledChanged();
    });

    CVarWrapper rewindLength = this->plugin->cvarManager->registerCvar("st_savestate_rewindlength", "6.0", "Rewind length");
    rewindLength.addOnValueChanged([this](const std::string &oldValue, const CVarWrapper &cvar) {
        this->onRewindLengthChanged();
    });
    this->rewindBuffer.setRewindLength(rewindLength.getFloatValue());

    this->plugin->cvarManager->registerCvar("st_savestate_rewindsaveinterval", "0.1", "Rewind save interval");

    this->plugin->cvarManager->registerNotifier("st_savestate_rewind", [this](const std::vector<std::string> &commands) {
        this->rewind();
    }, "", PERMISSION_PAUSEMENU_CLOSED | PERMISSION_FREEPLAY);

    MultiEventHooker::getInstance(this->plugin).hookEvent("Function TAGame.Car_TA.SetVehicleInput", [this](const std::string &eventName) {
        this->onPhysicsTick();
    });
}

void RewindStateComponent::onUnload()
{

}

void RewindStateComponent::render()
{
    bool isComponentEnabled = this->isComponentEnabled();
    if (ImGui::Checkbox("Rewind the game back in time", &isComponentEnabled))
    {
        this->plugin->gameWrapper->Execute([this, isComponentEnabled](GameWrapper *gw) {
            this->setComponentEnabled(isComponentEnabled);
        });
    }

    bool isInFreeplay = this->plugin->gameWrapper->IsInFreeplay();

    ImVec4 color = ImGui::GetStyle().Colors[isInFreeplay ? ImGuiCol_TextDisabled : ImGuiCol_Text];
    ImGui::TextColored(color, "(only works in freeplay and workshop maps)");

    ImGui::Spacing();

    ImGuiExtensions::PushDisabledStyleIf(!isComponentEnabled || !isInFreeplay);

    if (ImGui::Button("Rewind game state"))
    {
        this->plugin->gameWrapper->Execute([this](GameWrapper *gw) {
            this->rewind();
        });
    }

    float progress = this->rewindBuffer.progress();
    float frontOffset = this->rewindBuffer.frontOffset();
    ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), std::to_string(frontOffset).c_str());
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::Text("Rewind buffer (in seconds)");

    float length = this->getRewindLength();
    if (ImGui::SliderFloat("Rewind length (in seconds)", &length, 1.0f, 30.0f, "%.3f"))
    {
        this->plugin->gameWrapper->Execute([this, length](GameWrapper *gw) {
            this->setRewindLength(length);
        });
    }

    if (ImGui::Button("Default (6.0)"))
    {
        this->plugin->gameWrapper->Execute([this](GameWrapper *gw) {
            this->setRewindLength(6.0f);
        });
    }
    ImGui::SameLine();
    if (ImGui::Button("Short (2.0)"))
    {
        this->plugin->gameWrapper->Execute([this](GameWrapper *gw) {
            this->setRewindLength(2.0f);
        });
    }
    ImGui::SameLine();
    if (ImGui::Button("Long (12.0)"))
    {
        this->plugin->gameWrapper->Execute([this](GameWrapper *gw) {
            this->setRewindLength(12.0f);
        });
    }

//    if (ImGui::SliderFloat("Rewind save interval (in seconds)", this->rewindSaveInterval.get(), 0.001, 0.25, "%.3f"))
//    {
//        this->plugin->gameWrapper->Execute([this](GameWrapper *gw) {
//            this->setRewindSaveIntervalCVar(*this->rewindSaveInterval);
//        });
//    }

    ImGuiExtensions::PopDisabledStyleIf(!isComponentEnabled || !isInFreeplay);
}

void RewindStateComponent::rewind()
{
    if (!this->isComponentEnabled()) return;
    if (this->rewindBuffer.empty()) return;
    if (!this->plugin->gameWrapper->IsInFreeplay()) return;

    ServerWrapper server = this->plugin->gameWrapper->GetGameEventAsServer();
    if (server.IsNull()) return;

    this->rewindBuffer.removeAllButFront();
    GameState gameState = this->rewindBuffer.front();
    gameState.applyTo(server);
}


void RewindStateComponent::onPhysicsTick()
{
    if (!this->isComponentEnabled()) return;
    if (!this->plugin->gameWrapper->IsInFreeplay()) return;

    ServerWrapper server = this->plugin->gameWrapper->GetGameEventAsServer();
    if (server.IsNull()) return;

    float currentTime = server.GetSecondsElapsed();

    if (this->rewindBuffer.empty() || currentTime > previousSaveTime + this->getRewindSaveInterval())
    {
        GameState ss(server);
        this->rewindBuffer.push(ss);
        this->previousSaveTime = currentTime;
    }
}

bool RewindStateComponent::isComponentEnabled()
{
    return this->plugin->cvarManager->getCvar("st_savestate_rewind_enabled").getBoolValue();
}

void RewindStateComponent::setComponentEnabled(bool enabled)
{
    this->plugin->cvarManager->getCvar("st_savestate_rewind_enabled").setValue(enabled);
}

void RewindStateComponent::onComponentEnabledChanged()
{
    if (!this->isComponentEnabled())
    {
        this->rewindBuffer.clear();
    }
    this->plugin->gameWrapper->GetGameEventAsServer();
}

float RewindStateComponent::getRewindLength()
{
    return this->plugin->cvarManager->getCvar("st_savestate_rewindlength").getFloatValue();
}

void RewindStateComponent::setRewindLength(float length)
{
    this->plugin->cvarManager->getCvar("st_savestate_rewindlength").setValue(length);
}

void RewindStateComponent::onRewindLengthChanged()
{
    this->rewindBuffer.setRewindLength(this->getRewindLength());
}

float RewindStateComponent::getRewindSaveInterval()
{
    return this->plugin->cvarManager->getCvar("st_savestate_rewindsaveinterval").getFloatValue();
}

void RewindStateComponent::setRewindSaveInterval(float interval)
{
    this->plugin->cvarManager->getCvar("st_savestate_rewindsaveinterval").setValue(interval);
}
