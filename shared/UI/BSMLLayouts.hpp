#pragma once

// BSML markup for the settings menu. Embedded as a raw string so the .qmod
// doesn't need a separate asset-loading step -- BSML parses this into a UI
// tree at runtime and binds `value="X"` / `on-click="Y"` to public fields
// and methods on the host object by name (see SettingsViewController.hpp).
//
// VERIFY: RedBrumbler's Quest-BSML port accepts the same tag vocabulary as
// PC BSML (bool-setting, string-setting, slider-setting, dropdown-list-setting,
// list-setting, horizontal/vertical layout groups, button, text) as of the
// last time this was written, but check the tag list in the `bsml` package
// you restore -- Quest-BSML is an actively-developed independent port and
// individual tag support can lag or diverge from PC BSML slightly.

namespace YouTubeLiveChat::UI {

inline constexpr const char* kSettingsMenuXml = R"===(
<bg xsi:noNamespaceSchemaLocation="https://raw.githubusercontent.com/monkeymanboy/BSML-Docs/master/schemas/settingsViewController.xsd">
  <horizontal>
    <vertical pad-left="2" pad-right="2" spacing="1">
      <text text="YouTube Live Chat" font-size="5" align="Center"/>

      <string-setting text="API Key" value="ApiKey" on-change="OnApiKeyChanged" apply-on-change="true"/>
      <string-setting text="Video ID or URL" value="VideoIdOrUrl" on-change="OnVideoChanged" apply-on-change="true"/>

      <horizontal spacing="2" pad-top="1">
        <primary-button text="Connect" on-click="ConnectClicked"/>
        <button text="Disconnect" on-click="DisconnectClicked"/>
      </horizontal>
      <text text="~StatusText" align="Center" italics="true"/>

      <bool-setting text="Enable overlay" value="Enabled" on-change="OnEnabledChanged" apply-on-change="true"/>
      <bool-setting text="Auto-reconnect" value="AutoReconnect" apply-on-change="true"/>
      <bool-setting text="Debug logging" value="DebugLogging" apply-on-change="true"/>
    </vertical>

    <vertical pad-left="2" pad-right="2" spacing="1">
      <text text="Panel Placement" font-size="4" align="Center"/>
      <dropdown-list-setting text="Preset" value="PresetIndex" choices="PresetChoices" on-change="OnPresetChanged" apply-on-change="true"/>
      <slider-setting text="Horizontal offset" value="HorizontalOffset" min="-2" max="2" increment="0.05" apply-on-change="true"/>
      <slider-setting text="Vertical offset" value="VerticalOffset" min="-2" max="2" increment="0.05" apply-on-change="true"/>
      <slider-setting text="Distance" value="Distance" min="0.5" max="4" increment="0.1" apply-on-change="true"/>
      <slider-setting text="Width" value="Width" min="0.3" max="2.5" increment="0.05" apply-on-change="true"/>
      <slider-setting text="Height" value="Height" min="0.3" max="2.5" increment="0.05" apply-on-change="true"/>
      <slider-setting text="Scale" value="Scale" min="0.3" max="2.0" increment="0.05" apply-on-change="true"/>
      <slider-setting text="Opacity" value="Opacity" min="0.1" max="1.0" increment="0.05" apply-on-change="true"/>
    </vertical>

    <vertical pad-left="2" pad-right="2" spacing="1">
      <text text="Messages" font-size="4" align="Center"/>
      <slider-setting text="Max visible" value="MaxVisibleMessages" min="5" max="50" increment="1" integer-only="true" apply-on-change="true"/>
      <slider-setting text="Message duration (s, 0=never)" value="MessageDurationSeconds" min="0" max="180" increment="5" apply-on-change="true"/>

      <bool-setting text="Show regular messages" value="ShowRegularMessages" apply-on-change="true"/>
      <bool-setting text="Show Super Chats" value="ShowSuperChats" apply-on-change="true"/>
      <bool-setting text="Show Super Stickers" value="ShowSuperStickers" apply-on-change="true"/>
      <bool-setting text="Show memberships" value="ShowMembershipEvents" apply-on-change="true"/>
      <bool-setting text="Show usernames" value="ShowUsernames" apply-on-change="true"/>
      <bool-setting text="Show profile pictures" value="ShowProfilePictures" apply-on-change="true"/>
      <bool-setting text="Highlight Super Chats" value="HighlightSuperChats" apply-on-change="true"/>
      <bool-setting text="Highlight memberships" value="HighlightMemberships" apply-on-change="true"/>
      <bool-setting text="Notification sounds" value="NotificationSounds" apply-on-change="true"/>
    </vertical>
  </horizontal>
</bg>
)===";

}
