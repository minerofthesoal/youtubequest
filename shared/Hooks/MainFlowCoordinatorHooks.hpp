#pragma once

namespace YouTubeLiveChat::Hooks {

// Installs a hook on MainFlowCoordinator::DidActivate that spawns the
// overlay controller GameObject exactly once (guarded by ModState::OverlayPtr()
// already being non-null). MainFlowCoordinator is the first fully-initialized
// game object BSIPA/Scotland2-side mods reliably hook off of on both PC and
// Quest; using it (rather than, say, a gameplay-specific controller) is what
// lets the chat panel exist in the menu too, per requirement 7's "must work
// without requiring the player to leave the current song" + "avoid opening
// unnecessary menus" -- we never touch the menu flow at all, we just piggyback
// its activation as a reliable "the game has finished booting" signal.
void InstallHooks();

}
