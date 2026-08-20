#pragma once

namespace YouTubeLiveChat::Hooks {

// Installs:
//   * MainFlowCoordinator::DidActivate -- spawns the persistent overlay once
//     the game has finished booting. MainFlowCoordinator is the earliest
//     point that reliably means "Unity, il2cpp and the menu scene are all
//     up"; the overlay itself is DontDestroyOnLoad, so hooking a *menu* type
//     does not tie the overlay to the menu.
//   * PauseMenuManager::ShowMenu / StartResumeAnimation / MenuButtonPressed --
//     shows and hides the in-level control panel, so the mod stays usable
//     without leaving the song.
void InstallHooks();

}  // namespace YouTubeLiveChat::Hooks
