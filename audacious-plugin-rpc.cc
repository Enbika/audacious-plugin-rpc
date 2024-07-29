#include <iostream>
#include <string.h>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/hook.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/tuple.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#include <discord_rpc.h>
#include <cstdint>

#define EXPORT __attribute__((visibility("default")))
#define APPLICATION_ID "484736379171897344"

static const char *SETTING_EXTRA_TEXT = "extra_text";
static const char *SETTING_USE_PLAYING = "use_playing_status";

class RPCPlugin : public GeneralPlugin {

public:
    static const char about[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Discord RPC"),
        "audacious-plugin-rpc",
        about,
        &prefs
    };

    constexpr RPCPlugin() : GeneralPlugin (info, false) {}

    bool init();
    void cleanup();
};

EXPORT RPCPlugin aud_plugin_instance;

DiscordEventHandlers handlers;
DiscordRichPresence presence;
std::string title;
std::string titleText;
std::string fullTitle;
std::string artist;
std::string artistText;
std::string album;
std::string albumText;
std::string state;
std::string playingStatus;
std::int64_t length;
std::int64_t timestamp;

void init_discord() {
    memset(&handlers, 0, sizeof(handlers));
    Discord_Initialize(APPLICATION_ID, &handlers, 1, NULL);
}

void update_presence() {
    Discord_UpdatePresence(&presence);
}

void init_presence() {
    memset(&presence, 0, sizeof(presence));
    if(!aud_get_bool("audacious-plugin-rpc", SETTING_USE_PLAYING))
        presence.type = DiscordActivityType_Listening;
    presence.startTimestamp = time(NULL);
    update_presence();
}

void cleanup_discord() {
    Discord_ClearPresence();
    Discord_Shutdown();
}

void title_changed() {
    if(aud_get_bool("audacious-plugin-rpc", SETTING_USE_PLAYING))
        presence.type = DiscordActivityType_Playing;
    else
        presence.type = DiscordActivityType_Listening;
    presence.largeImageKey = "logo";

    if (aud_drct_get_ready() && aud_drct_get_playing()) {
        bool paused = aud_drct_get_paused();
        Tuple tuple = aud_drct_get_tuple();
        title = tuple.get_str(Tuple::Title);
        
        titleText = title.substr(0, 127);

        String artistString = tuple.get_str(Tuple::Artist);
        if(artistString) {
            artist = tuple.get_str(Tuple::Artist);
            artistText = artist.substr(0, 127);
            fullTitle = (title + " - " + artist).substr(0, 127);
        } else {
            fullTitle = title.substr(0, 127);
            artistText = "";
        }
        
        String albumString = tuple.get_str(Tuple::Album);
        if(albumString) {
            album = tuple.get_str(Tuple::Album);
            albumText = album.substr(0, 127);
        } else {
            albumText = "";
        }

        length = tuple.get_int(Tuple::Length);
        timestamp = (length / 1000) - (aud_drct_get_time() / 1000);

        playingStatus = paused ? "Paused" : "Listening";

        if(aud_get_bool("audacious-plugin-rpc", SETTING_USE_PLAYING)) {
            presence.details = fullTitle.c_str();
            presence.state = albumText.c_str();
            presence.largeImageText = "";
        } else {
            presence.details = titleText.c_str();
            presence.state = artistText.c_str();
            presence.largeImageText = albumText.c_str();
        }

        presence.smallImageKey = paused ? "pause" : "play";
        presence.startTimestamp = paused ? 0 : (time(NULL) - aud_drct_get_time() / 1000);
        presence.endTimestamp = (paused || length == -1) ? 0 : time(NULL) + timestamp;
    } else {
        playingStatus = "Stopped";
        presence.details = "Stopped";
        presence.state = "";
        presence.largeImageText = "";
        presence.smallImageKey = "stop";
        presence.startTimestamp = 0;
        presence.endTimestamp = 0;
    }

    std::string extraText(aud_get_str("audacious-plugin-rpc", SETTING_EXTRA_TEXT));
    playingStatus = (playingStatus + " " + extraText).substr(0, 127);
    
    presence.smallImageText = playingStatus.c_str();
    update_presence();
}

void update_title_presence(void*, void*) {
    title_changed();
}

void open_github() {
   system("xdg-open https://github.com/darktohka/audacious-plugin-rpc");
}

bool RPCPlugin::init() {
    init_discord();
    init_presence();
    hook_associate("playback ready", update_title_presence, nullptr);
    hook_associate("playback end", update_title_presence, nullptr);
    hook_associate("playback stop", update_title_presence, nullptr);
    hook_associate("playback pause", update_title_presence, nullptr);
    hook_associate("playback unpause", update_title_presence, nullptr);
    hook_associate("playback seek", update_title_presence, nullptr);
    hook_associate("playlist end reached", update_title_presence, nullptr);
    hook_associate("title change", update_title_presence, nullptr);
    return true;
}

void RPCPlugin::cleanup() {
    hook_dissociate("playback ready", update_title_presence);
    hook_dissociate("playback end", update_title_presence);
    hook_dissociate("playback stop", update_title_presence);
    hook_dissociate("playback pause", update_title_presence);
    hook_dissociate("playback unpause", update_title_presence);
    hook_dissociate("playback seek", update_title_presence);
    hook_dissociate("playlist end reached", update_title_presence);
    hook_dissociate("title change", update_title_presence);
    cleanup_discord();
}

const char RPCPlugin::about[] = N_("Discord RPC music status plugin\n\nWritten by: Derzsi Daniel <daniel@tohka.us>");

const PreferencesWidget RPCPlugin::widgets[] =
{
  WidgetEntry(
      N_("Extra status text:"),
      WidgetString("audacious-plugin-rpc", SETTING_EXTRA_TEXT, title_changed)
  ),
  WidgetCheck(
      N_("Use \"Playing\" status"),
      WidgetBool("audacious-plugin-rpc", SETTING_USE_PLAYING, title_changed)
  ),
  WidgetButton(
      N_("Fork on GitHub"),
      {open_github}
  )
};

const PluginPreferences RPCPlugin::prefs = {{ widgets }};
