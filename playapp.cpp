#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <Application.h>
#include <Lepra.h>
#include <LogListener.h>
#include <MemberThread.h>
#include <Network.h>
#include <Socket.h>
#include <SystemManager.h>
#include <Log.h>
#include "api.h"
#include "audio.h"

#define doexit(x) SystemManager::ExitProcess(x)


/* --- Data --- */
const uint8_t g_appkey[] = {
	0x01, 0xBC, 0xFA, 0x06, 0x7A, 0xF9, 0xC0, 0x6D, 0x30, 0x93, 0xEF, 0x54, 0x69, 0xD0, 0x52, 0x6A,
	0xA6, 0x70, 0xE4, 0xC7, 0xBE, 0xB4, 0x76, 0x4F, 0xC1, 0x57, 0x2D, 0xBE, 0x63, 0xEC, 0x59, 0xEE,
	0x72, 0xEA, 0xF4, 0x88, 0xBE, 0xE8, 0x48, 0x6E, 0xE6, 0x36, 0xD8, 0x22, 0xEC, 0x1A, 0xE3, 0x3C,
	0xDB, 0x32, 0xAA, 0xE8, 0x33, 0xE0, 0xEC, 0x19, 0x4E, 0x78, 0xF6, 0x7F, 0x33, 0x84, 0x5B, 0x62,
	0xD3, 0x1E, 0x1C, 0x33, 0xDC, 0x22, 0x58, 0x1E, 0x10, 0x1D, 0xB7, 0x05, 0xED, 0x82, 0x58, 0x59,
	0x97, 0x2D, 0x73, 0xE8, 0x98, 0x39, 0xCE, 0x1D, 0x1B, 0x4C, 0xCE, 0x7E, 0xB9, 0x6A, 0x6B, 0x2E,
	0xED, 0xD8, 0x28, 0x64, 0x9A, 0x07, 0x11, 0x4F, 0x86, 0x69, 0xD8, 0x3F, 0x04, 0x0E, 0x94, 0x8E,
	0xB6, 0xD3, 0x8A, 0x60, 0x24, 0xBD, 0xAB, 0x65, 0xE3, 0x52, 0x06, 0xE4, 0x6F, 0x61, 0x8A, 0x5B,
	0x8C, 0xC5, 0x92, 0x07, 0xFA, 0x61, 0x07, 0x2B, 0x0E, 0x19, 0x44, 0xB7, 0x36, 0x97, 0xD8, 0x39,
	0x8C, 0x6A, 0xD8, 0x5F, 0x12, 0x17, 0x92, 0x44, 0x5E, 0xF0, 0x12, 0x48, 0xB6, 0xCC, 0xE5, 0x36,
	0x6B, 0x5C, 0x78, 0x16, 0x40, 0x65, 0x0C, 0x0B, 0x88, 0x35, 0x4F, 0x97, 0x56, 0xA6, 0x51, 0xDC,
	0x13, 0xCA, 0xB1, 0x5C, 0xA0, 0xAC, 0x8F, 0x66, 0x12, 0x0B, 0xF9, 0x28, 0x33, 0xE4, 0x6F, 0x92,
	0x75, 0x0E, 0xE8, 0x86, 0x92, 0x00, 0xE5, 0x0D, 0x0B, 0x1F, 0x9C, 0x2D, 0x95, 0xD8, 0xA4, 0xBB,
	0x16, 0x13, 0x23, 0x4F, 0x76, 0x1E, 0x8E, 0x01, 0x15, 0xE7, 0x53, 0x33, 0xAC, 0xCD, 0x14, 0x97,
	0x66, 0x44, 0xDB, 0x62, 0x84, 0x6E, 0x27, 0x4C, 0xDD, 0x05, 0x86, 0xCB, 0xFB, 0xDC, 0x18, 0xC3,
	0xDA, 0x39, 0x21, 0x5B, 0x2C, 0x82, 0x1C, 0xCB, 0x4D, 0x9A, 0xED, 0x34, 0x06, 0x02, 0xB8, 0xB4,
	0xE8, 0x8A, 0xC9, 0x41, 0x07, 0xB9, 0xB2, 0x19, 0x4A, 0xDF, 0x98, 0xD6, 0x5B, 0xEA, 0x62, 0xC1,
	0x9C, 0x16, 0xBF, 0x11, 0xAF, 0x4F, 0x42, 0x52, 0x5D, 0xD9, 0x99, 0xE2, 0xD3, 0x44, 0xC1, 0x61,
	0x56, 0xF6, 0xA9, 0x45, 0x66, 0x1D, 0x28, 0xAC, 0x5A, 0x1E, 0x16, 0x52, 0x28, 0x18, 0x92, 0xF6,
	0x06, 0xDA, 0x02, 0x26, 0xB0, 0x3B, 0x69, 0x47, 0x95, 0x95, 0x65, 0x8B, 0xE1, 0x91, 0xE6, 0x43,
	0xA4,
};
const size_t g_appkey_size = sizeof(g_appkey);

/// The output queue for audo data
static audio_fifo_t g_audiofifo;
/// Synchronization mutex for the main thread
static FastLock g_notify_mutex;
/// Synchronization condition variable for the main thread
static FastCondition g_notify_cond(&g_notify_mutex);
/// Synchronization variable telling the main thread to process events
static int g_notify_do = 0;
/// Non-zero when a track has ended and the jukebox has not yet started a new one
static int g_playback_done = 0;
/// The global session handle
static sp_session* g_sess = 0;
/// Handle to the playlist currently being played
//static sp_playlist *g_jukeboxlist;
/// Name of the playlist currently being played
//const char *g_listname;
/// Remove tracks flag
//static int g_remove_tracks = 0;
/// Handle to the curren track
static sp_track* g_currenttrack = 0;
/// Index to the next track
//static int g_track_index;
static bool isplaying = false;
static bool hurry = false;
UdpSocket* command_socket = 0;
SocketAddress last_remote_address;


static void print(const astr& msg)
{
	puts(msg.c_str());
	fflush(stdout);

	if (last_remote_address.GetPort() != 0)
	{
		if (command_socket->SendTo((const uint8*)msg.c_str(), (int)msg.length(), last_remote_address) != (int)msg.length())
		{
			last_remote_address.SetPort(0);
		}
	}
}

static void SP_CALLCONV logged_in(sp_session *sess, sp_error error)
{
	if (SP_ERROR_OK != error) {
		print(astrutil::Format("Login error: %s\n", sp_error_message(error)));
		doexit(2);
	}
	print("Login ok.\n");
}


static void play_song(const astr& song_link)
{
	sp_link* link = sp_link_create_from_string(song_link.c_str());
        sp_track_add_ref(g_currenttrack = sp_link_as_track(link));
	sp_link_release(link);
	sp_error e = sp_track_error(g_currenttrack);
	if (e == SP_ERROR_OK) {
		print(astrutil::Format("Playing \"%s\"...\n", sp_track_name(g_currenttrack)));
		sp_session_player_load(g_sess, g_currenttrack);
		sp_session_player_play(g_sess, 1);
		isplaying = true;
		hurry = false;
        }
}

static void SP_CALLCONV search_complete(sp_search *search, void *userdata)
{
	if (sp_search_error(search) != SP_ERROR_OK)
	{
		print(astrutil::Format("Search error: %s\n", sp_error_message(sp_search_error(search))));
		hurry = false;
		return;
	}

	for (int i = 0; i < sp_search_num_tracks(search); ++i)
	{
		sp_track* track = sp_search_track(search, i);
		const astr name = sp_track_name(track);
		const int popularity = sp_track_popularity(track);
		astrutil::strvec artists;
		for (int j = 0; j < sp_track_num_artists(track); ++j) {
			sp_artist* art = sp_track_artist(track, j);
			artists.push_back(sp_artist_name(art));
		}
		const astr artists_str = astrutil::Join(artists, "~");
		sp_link* l = sp_link_create_from_track(track, 0);
		char url[256];
		sp_link_as_string(l, url, sizeof(url));
		print(astrutil::Format("song %s ~~~ %i ~~~ %s ~~~ %s\n", name.c_str(), popularity, artists_str.c_str(), url));
	}

	for (int i = 0; i < sp_search_num_artists(search); ++i)
	{
		sp_artist* artist = sp_search_artist(search, i);
		const astr name = sp_artist_name(artist);
		print(astrutil::Format("artist %s\n", name.c_str()));
	}

	sp_search_release(search);

	hurry = false;
}

static void SP_CALLCONV toplistbrowse_complete(sp_toplistbrowse *result, void *userdata)
{
	if (sp_toplistbrowse_error(result) != SP_ERROR_OK)
	{
		print(astrutil::Format("Toplist browse error: %s\n", sp_error_message(sp_toplistbrowse_error(result))));
		hurry = false;
		return;
	}

	for (int i = 0; i < sp_toplistbrowse_num_tracks(result); ++i)
	{
		sp_track* track = sp_toplistbrowse_track(result, i);
		const astr name = sp_track_name(track);
		const int popularity = sp_track_popularity(track);
		astrutil::strvec artists;
		for (int j = 0; j < sp_track_num_artists(track); ++j) {
			sp_artist* art = sp_track_artist(track, j);
			artists.push_back(sp_artist_name(art));
		}
		const astr artists_str = astrutil::Join(artists, "~");
		sp_link* l = sp_link_create_from_track(track, 0);
		char url[256];
		sp_link_as_string(l, url, sizeof(url));
		print(astrutil::Format("song %s ~~~ %i ~~~ %s ~~~ %s\n", name.c_str(), popularity, artists_str.c_str(), url));
	}
	hurry = false;
}

static void search_song(const astr& name)
{
	sp_search_create(g_sess, name.c_str(), 0, 20, 0, 0, 0, 0, 0, 0, SP_SEARCH_STANDARD, &search_complete, NULL);
}

static void search_artist(const astr& name)
{
	sp_search_create(g_sess, name.c_str(), 0, 0, 0, 0, 0, 5, 0, 0, SP_SEARCH_STANDARD, &search_complete, NULL);
}

static void search_toplist()
{
	sp_toplistbrowse_create(g_sess, SP_TOPLIST_TYPE_TRACKS, SP_TOPLIST_REGION_EVERYWHERE, NULL, toplistbrowse_complete, NULL);
}

/**
 * This callback is called from an internal libspotify thread to ask us to
 * reiterate the main loop.
 *
 * We notify the main thread using a condition variable and a protected variable.
 *
 * @sa sp_session_callbacks#notify_main_thread
 */
static void SP_CALLCONV notify_main_thread(sp_session *sess)
{
	g_notify_mutex.Acquire();
	g_notify_do = 1;
	g_notify_cond.Signal();
	g_notify_mutex.Release();
}

/**
 * This callback is used from libspotify whenever there is PCM data available.
 *
 * @sa sp_session_callbacks#music_delivery
 */
static int SP_CALLCONV music_delivery(sp_session *sess, const sp_audioformat *format,
                          const void *frames, int num_frames)
{
	audio_fifo_t* af = &g_audiofifo;
	audio_fifo_data_t* afd;
	size_t s;

	if (num_frames == 0)
		return 0; // Audio discontinuity, do nothing

	af->mutex.Acquire();

	/* Buffer one second of audio */
	if (af->qlen > format->sample_rate) {
		af->mutex.Release();
		return 0;
	}

	s = num_frames * sizeof(int16_t) * format->channels;

	afd = (audio_fifo_data_t*)malloc(sizeof(*afd) + s);
	memcpy(afd->samples, frames, s);

	afd->nsamples = num_frames;

	afd->rate = format->sample_rate;
	afd->channels = format->channels;

	af->q.push_back(afd);
	af->qlen += num_frames;

	af->cond->Signal();
	af->mutex.Release();

	return num_frames;
}


/**
 * This callback is used from libspotify when the current track has ended
 *
 * @sa sp_session_callbacks#end_of_track
 */
static void SP_CALLCONV end_of_track(sp_session *sess)
{
	g_notify_mutex.Acquire();
	g_playback_done = 1;
	g_notify_do = 1;
	g_notify_cond.Signal();
	g_notify_mutex.Release();
}


/**
 * Callback called when libspotify has new metadata available
 *
 * Not used in this example (but available to be able to reuse the session.c file
 * for other examples.)
 *
 * @sa sp_session_callbacks#metadata_updated
 */
static void SP_CALLCONV metadata_updated(sp_session *sess)
{
	if (isplaying || !g_currenttrack || sp_track_error(g_currenttrack) != SP_ERROR_OK)
		return;

	print(astrutil::Format("Playing \"%s\"...\n", sp_track_name(g_currenttrack)));

	sp_session_player_load(g_sess, g_currenttrack);
	sp_session_player_play(g_sess, 1);
	isplaying = true;
	hurry = false;
}

/**
 * Notification that some other connection has started playing on this account.
 * Playback has been stopped.
 *
 * @sa sp_session_callbacks#play_token_lost
 */
static void SP_CALLCONV play_token_lost(sp_session *sess)
{
	print("Play token lost! Someone else using this account?\n");

	audio_fifo_flush(&g_audiofifo);

	if (g_currenttrack != NULL) {
		sp_session_player_unload(g_sess);
		g_currenttrack = NULL;
		isplaying = false;
		hurry = false;
	}
}

static void SP_CALLCONV log_message(sp_session *session, const char *msg)
{
	puts(msg);
}

/**
 * The session callbacks
 */
static sp_session_callbacks session_callbacks;

/**
 * The session configuration
 */
static sp_session_config spconfig;

/* -------------------------  END SESSION CALLBACKS  ----------------------- */


/**
 * A track has ended. Remove it from the playlist.
 *
 * Called from the main loop when the music_delivery() callback has set g_playback_done.
 */
static void SP_CALLCONV track_ended(void)
{
	if (g_currenttrack)
	{
		sp_track_release(g_currenttrack);
		g_currenttrack = NULL;
	}
	sp_session_player_unload(g_sess);
	isplaying = false;
	hurry = false;
}

static void SP_CALLCONV stop(void)
{
	track_ended();
	audio_fifo_flush(&g_audiofifo);
	print("Stopped.\n");
}

class PlayApp: public Application
{
public:
	PlayApp(const strutil::strvec& pArgumentList);
	virtual void Init();
	int Run();
	void Usage();

private:
	void HandleStdinCmds();
	void HandleSockCmds();
	bool HandleCmd(const astr& cmd);

	MemberThread<PlayApp> mStdinCmdThread;
	MemberThread<PlayApp> mSockCmdThread;
	std::list<astr> cmds;
};

LEPRA_RUN_APPLICATION(PlayApp, Main);

PlayApp::PlayApp(const strutil::strvec& args):
	Application(args),
	mStdinCmdThread("StdinCmdThread"),
	mSockCmdThread("SockCmdThread")
{
	//Thread::Sleep(10.0);
	//print("doin summat");
	Lepra::Init();
	Network::Start();

	if (args.size() != 3)
	{
		Usage();
		doexit(1);
	}
}

void PlayApp::Init()
{
	SocketAddress lLocalAddress;
	lLocalAddress.Resolve(_T("localhost:55552"));
	command_socket = new UdpSocket(lLocalAddress, true);
	if (!command_socket->IsOpen())
	{
		print("Error: socket already opened!\n");
		doexit(1);
	}
	mSockCmdThread.Start(this, &PlayApp::HandleSockCmds);
	mStdinCmdThread.Start(this, &PlayApp::HandleStdinCmds);
	Thread::Sleep(0.3);

	session_callbacks.logged_in = &logged_in;
	session_callbacks.notify_main_thread = &notify_main_thread;
	session_callbacks.music_delivery = &music_delivery;
	session_callbacks.metadata_updated = &metadata_updated;
	session_callbacks.play_token_lost = &play_token_lost;
	session_callbacks.log_message = NULL;
	session_callbacks.end_of_track = &end_of_track;
	spconfig.api_version = SPOTIFY_API_VERSION;
	spconfig.cache_location = "tmp";
	spconfig.settings_location = "tmp";
	spconfig.application_key = g_appkey;
	spconfig.application_key_size = g_appkey_size;
	spconfig.user_agent = "consoleify";
	spconfig.callbacks = &session_callbacks;
	spconfig.userdata = NULL;
	static astr puser,ppass,purl;
	DiskFile f;
	if (f.Open(_T("proxy_settings.txt"), DiskFile::MODE_TEXT_READ))
	{
		astr s;
		f.ReadLine(s);
		s = astrutil::Strip(s, " \t\n\r");
		astrutil::strvec scheme_base = astrutil::Split(s, "://", 1);
		astr scheme = scheme_base[0];
		astr base = scheme_base[1];
		astrutil::strvec user_hostport = astrutil::Split(base, "@", 1);
		astrutil::strvec user_pass = astrutil::Split(user_hostport[0], ":", 1);
		puser = user_pass[0];
		ppass = user_pass[1];
		purl = scheme + "://" + user_hostport[1];
	}
	spconfig.proxy = purl.empty()? NULL : purl.c_str();
	spconfig.proxy_username = puser.empty()? NULL : puser.c_str();
	spconfig.proxy_password = ppass.empty()? NULL : ppass.c_str();

	audio_init(&g_audiofifo);
}

void PlayApp::HandleStdinCmds()
{
	while (!std::cin.eof() && !mStdinCmdThread.GetStopRequest())
	{
		char c[1024];
		std::cin.getline(c, sizeof(c));
		g_notify_mutex.Acquire();
		cmds.push_back(astrutil::StripRight(c, "\r\n \t"));
		g_notify_cond.Signal();
		g_notify_mutex.Release();
	}
}

void PlayApp::HandleSockCmds()
{
	while (!mSockCmdThread.GetStopRequest())
	{
		uint8 data[1024];
		const int l = command_socket->ReceiveFrom(data, sizeof(data)-1, last_remote_address, 5);
		if (mSockCmdThread.GetStopRequest())
		{
			break;
		}
		if (l <= 0 || data[l-1] != '\n')
		{
			last_remote_address.SetPort(0);
			command_socket->ClearErrors();
			continue;
		}
		const astr cmd = astr((char*)data, l-1);
		if (cmd == "ping")
		{
			print("pong\n");
			mStdinCmdThread.RequestStop();
			continue;
		}
		g_notify_mutex.Acquire();
		cmds.push_back(cmd);
		g_notify_cond.Signal();
		g_notify_mutex.Release();
	}

	g_notify_mutex.Acquire();
	cmds.push_back("quit");
	g_notify_cond.Signal();
	g_notify_mutex.Release();
}

bool PlayApp::HandleCmd(const astr& cmd)
{
	if (cmd.empty())
	{
		return false;
	}

	hurry = true;
	astrutil::strvec words = astrutil::BlockSplit(cmd, " \t", false, false, 1);
	const astr acmd = (words.size() >= 1)? words[0] : "";
	astr args = (words.size() >= 2)? words[1] : "";
	if (acmd == "play-song" && !args.empty())
	{
		play_song(args);
	}
	else if (acmd == "is-playing")
	{
		print(isplaying? "yes\n" : "no\n");
	}
	else if (acmd == "search-song" && !args.empty())
	{
		search_song(args);
	}
	else if (acmd == "search-artist" && !args.empty())
	{
		search_artist(args);
	}
	else if (acmd == "toplist")
	{
		search_toplist();
	}
	else if (acmd == "stop")
	{
		stop();
	}
	else if (acmd == "quit")
	{
		sp_session_logout(g_sess);
		g_sess = 0;
		doexit(0);
	}
	else
	{
		hurry = false;
		print(astrutil::Format("Unknown command '%s'.", acmd.c_str()));
		return false;
	}
	return true;
}

int PlayApp::Run()
{
	sp_session *sp;
	sp_error err;
	int next_timeout = 0;
	const astr username = astrutil::Encode(SystemManager::GetArgumentVector()[1]);
	const astr password = astrutil::Encode(SystemManager::GetArgumentVector()[2]);

	/* Create session */
	err = sp_session_create(&spconfig, &sp);
	if (SP_ERROR_OK != err) {
		print(astrutil::Format("Error creating session: %s\n", sp_error_message(err)));
		doexit(1);
	}
	g_sess = sp;
	sp_session_login(sp, username.c_str(), password.c_str(), 0, NULL);

	g_notify_mutex.Acquire();

	for (;;) {
		if (next_timeout == 0)
		{
			while(!g_notify_do)
			{
				g_notify_cond.Wait();
			}
		}
		else
		{
			g_notify_cond.Wait(next_timeout/1000.0);
		}

		g_notify_do = 0;
		g_notify_mutex.Release();

		if (g_playback_done) {
			track_ended();
			g_playback_done = 0;
		}

		while (!cmds.empty())
		{
			g_notify_mutex.Acquire();
			astr cmd = cmds.front();
			cmds.pop_front();
			g_notify_mutex.Release();
			HandleCmd(cmd);
		}

		do {
			sp_session_process_events(sp, &next_timeout);
		} while (next_timeout == 0);
		const int maxt = hurry? 100 : 10000;
		next_timeout = (next_timeout>maxt)? maxt : next_timeout;

		g_notify_mutex.Acquire();
	}

	return 0;
}

void PlayApp::Usage()
{
	print(astrutil::Format("usage: %s <username> <password>", astrutil::Encode(SystemManager::GetArgumentVector()[0]).c_str()));
}
