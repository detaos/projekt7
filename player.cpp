#include "player.h"

#include <QDateTime>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QProgressDialog>
#include <QVBoxLayout>

#include <KActionCollection>
#include <KApplication>
#include <KConfigGroup>
#include <KFileDialog>
#include <KLocale>
#include <KMessageBox>
#include <KSeparator>
#include <KStandardAction>
#include <KStandardDirs>
#include <KStatusBar>
#include <KToolBar>

#include <phonon/audiooutput.h>
#include <phonon/mediaobject.h>
#include <phonon/mediasource.h>
#include <phonon/path.h>
#include <phonon/seekslider.h>
#include <phonon/volumeslider.h>

#include <taglib/tag.h>
#include <taglib/fileref.h>

#define qtos(q) (q).toStdString().c_str()
#define qsnb(q) (q).toUtf8().size()
#define formatTime(t) ((t) / 60000) << ':' << qSetFieldWidth(2) << qSetPadChar('0') << right << ((t) / 1000) % 60

const int SONG_NAME   = 0;
const char *ALL = "[All]";

/*
 * TABLE DEF:
 *  col   Name          Type      Key
 *  0     tid           INTEGER   PRIMARY ASC
 *  1     artist        VARCHAR   ASC
 *  2     year          INT       ASC
 *  3     album         VARCHAR
 *  4     track_number  INT       ASC
 *  5     title         VARCHAR
 *  6     path          VARCHAR
 */

Player::Player(QWidget *parent) : KXmlGuiWindow(parent) {
	//SETUP DATABASE
	QDir(KGlobal::dirs()->saveLocation("data")).mkdir("projekt7"); //NOTE: creates the projekt7 directory if it doesn't already exist
	QString db_path = KGlobal::dirs()->saveLocation("data") + "projekt7/tracks_db";
	int return_code = sqlite3_open(qtos(db_path), &tracks_db);
	if (return_code) {
		showError("Failed to open the Projekt7 Track Database: ", sqlite3_errmsg(tracks_db));
		exit(return_code);
	}
	const char *create_table = "CREATE TABLE IF NOT EXISTS `tracks` (`tid` INTEGER PRIMARY KEY, `artist` VARCHAR KEY ASC, `year` INT KEY ASC, `album` VARCHAR, `track_number` INT KEY ASC, `title` VARCHAR, `path` VARCHAR, `length` INT, `playcount` INT)"; //TODO make use of the `length` and `playcount` columns
	char *errmsg;
	return_code = sqlite3_exec(tracks_db, create_table, 0, 0, &errmsg);
	if (return_code) {
		showError("Failed to create `tracks` table: ", errmsg);
		sqlite3_free(errmsg);
		exit(return_code);
	}
	updateNumTracks();
	
	//SETUP PHONON
	now_playing = new Phonon::MediaObject(this);
	Phonon::AudioOutput *audioOutput = new Phonon::AudioOutput(Phonon::MusicCategory, this);
	createPath(now_playing, audioOutput);
	now_playing->setTickInterval(1000);
	
	//SETUP WIDGETS
	QWidget *central_widget = new QWidget(this);
	KToolBar *toolbar_widget = new KToolBar(i18n("Main Toolbar"), central_widget);
	toolbar_widget->setIconDimensions(32);
	playlist_widget = new QWidget(central_widget);
	artist_list = new KListWidget(playlist_widget);
	artist_list->addItem(ALL);
	album_list = new KListWidget(playlist_widget);
	titles_list = new KListWidget(playlist_widget);
	QAction* tb_previousAction = toolbar_widget->addAction(KIcon("media-skip-backward"), "");
	QString previousHelpText = i18n("Play the previous track");
	tb_previousAction->setToolTip(previousHelpText);
	QAction* tb_pauseAction = toolbar_widget->addAction(KIcon("media-playback-pause"), "");
	QString plauseHelpText = i18n("Plause playback");
	tb_pauseAction->setToolTip(plauseHelpText);
	QAction* tb_playAction = toolbar_widget->addAction(KIcon("media-playback-start"), "");
	QString playHelpText = i18n("Play the track");
	tb_playAction->setToolTip(playHelpText);
	QAction* tb_nextAction = toolbar_widget->addAction(KIcon("media-skip-forward"), "");
	QString nextHelpText = i18n("Play the next track");
	tb_nextAction->setToolTip(nextHelpText);
	toolbar_widget->addSeparator();
	Phonon::VolumeSlider *volumeSlider = new Phonon::VolumeSlider;
	volumeSlider->setAudioOutput(audioOutput);
	volumeSlider->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	toolbar_widget->addWidget(volumeSlider);
	toolbar_widget->addSeparator();
	Phonon::SeekSlider *seekSlider = new Phonon::SeekSlider(now_playing);
	volumeSlider->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	toolbar_widget->addWidget(seekSlider);
	toolbar_widget->addSeparator();
	cur_time = new QLabel(" 0:00", toolbar_widget);
	cur_time->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	toolbar_widget->addWidget(cur_time);
	toolbar_widget->addWidget(new QLabel(" / ", toolbar_widget));
	track_duration = new QLabel("0:00", toolbar_widget);
	track_duration->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	toolbar_widget->addWidget(track_duration);
	queued = new KIcon("go-next-view");
	
	QHBoxLayout *listLayout = new QHBoxLayout;
	listLayout->addWidget(artist_list);
	listLayout->addWidget(album_list);
	listLayout->addWidget(titles_list);
	playlist_widget->setLayout(listLayout);
	
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(toolbar_widget);
	mainLayout->addWidget(playlist_widget);
	central_widget->setLayout(mainLayout);
	
	setCentralWidget(central_widget);
	
	//SETUP STATUS BAR
	KStatusBar* bar = statusBar();
	bar->insertPermanentItem("", SONG_NAME, true);
	bar->setItemAlignment(SONG_NAME, Qt::AlignLeft | Qt::AlignVCenter);
	
	//SETUP SYSTEM TRAY ICON
	tray_icon = new KSystemTrayIcon("projekt7", this);
	tray_icon->show();
	
	//SETUP METADATA WINDOW
	metadata_window = new QWidget(this, Qt::Dialog);
	metadata_window->setWindowModality(Qt::WindowModal);
	metadata_window->setWindowTitle("Track Details  |  Projekt 7");
	mw_ok_button = new KPushButton(KIcon("dialog-ok-apply"), "OK", metadata_window);
	mw_ok_button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	
	QGridLayout *mwLayout = new QGridLayout;
	mwLayout->addWidget(new QLabel("Artist:",        metadata_window), 0, 0);
	mwLayout->addWidget(new QLabel("Year:",          metadata_window), 1, 0);
	mwLayout->addWidget(new QLabel("Album:",         metadata_window), 2, 0);
	mwLayout->addWidget(new QLabel("Track Number:",  metadata_window), 3, 0);
	mwLayout->addWidget(new QLabel("Title:",         metadata_window), 4, 0);
	mwLayout->addWidget(new QLabel("File Path:",     metadata_window), 5, 0);
	mwLayout->addWidget(mw_artist       = new QLabel(metadata_window), 0, 1);
	mwLayout->addWidget(mw_year         = new QLabel(metadata_window), 1, 1);
	mwLayout->addWidget(mw_album        = new QLabel(metadata_window), 2, 1);
	mwLayout->addWidget(mw_track_number = new QLabel(metadata_window), 3, 1);
	mwLayout->addWidget(mw_title        = new QLabel(metadata_window), 4, 1);
	mwLayout->addWidget(mw_path         = new QLabel(metadata_window), 5, 1);
	mwLayout->addWidget(mw_ok_button, 6, 0, 1, 2, Qt::AlignCenter);
	metadata_window->setLayout(mwLayout);
	
	//SETUP QUEUE EDITOR WINDOW
	queue_window = new QWidget(this, Qt::Dialog);
	queue_window->setWindowModality(Qt::WindowModal);
	queue_window->setWindowTitle("Track Queue Editor  |  Projekt 7");
	qw_ok_button = new KPushButton(KIcon("dialog-ok-apply"), "OK", queue_window);
	qw_ok_button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	
	QVBoxLayout *qw_buttons_layout = new QVBoxLayout;
	KPushButton *qw_top_button     = new KPushButton(KIcon("go-top"),      "", queue_window);
	KPushButton *qw_up_button      = new KPushButton(KIcon("go-up"),       "", queue_window);
	KPushButton *qw_down_button    = new KPushButton(KIcon("go-down"),     "", queue_window);
	KPushButton *qw_bottom_button  = new KPushButton(KIcon("go-bottom"),   "", queue_window);
	KPushButton *qw_remove_button  = new KPushButton(KIcon("edit-delete"), "", queue_window);
	qw_buttons_layout->addWidget(qw_top_button);
	qw_buttons_layout->addWidget(qw_up_button);
	qw_buttons_layout->addWidget(qw_down_button);
	qw_buttons_layout->addWidget(qw_bottom_button);
	qw_buttons_layout->addWidget(qw_remove_button);
	
	QHBoxLayout *qw_list_buttons_layout = new QHBoxLayout;
	qw_queue_list = new KListWidget(queue_window);
	QWidget *qw_button_holder = new QWidget(queue_window);
	qw_button_holder->setLayout(qw_buttons_layout);
	qw_list_buttons_layout->addWidget(qw_queue_list);
	qw_list_buttons_layout->addWidget(qw_button_holder);
	
	QVBoxLayout *qwLayout = new QVBoxLayout;
	QWidget *qw_list_buttons = new QWidget(queue_window);
	qw_list_buttons->setLayout(qw_list_buttons_layout);
	qwLayout->addWidget(qw_list_buttons);
	qwLayout->addWidget(qw_ok_button, 0, Qt::AlignCenter);
	queue_window->setLayout(qwLayout);
	
	//SETUP ACTIONS
 	KStandardAction::quit(kapp, SLOT(quit()), actionCollection());
	connect(kapp, SIGNAL(aboutToQuit()), this, SLOT(quit()));
	connect(now_playing, SIGNAL(aboutToFinish()), this, SLOT(enqueueNext()));
	connect(now_playing, SIGNAL(totalTimeChanged(qint64)), this, SLOT(updateDuration(qint64)));
	connect(now_playing, SIGNAL(tick(qint64)), this, SLOT(tick(qint64)));
	KAction *openFilesAction = setupKAction("document-open", i18n("Open"), i18n("Load the selected files"), "files");
	openFilesAction->setShortcut(QKeySequence::Open);
	connect(openFilesAction, SIGNAL(triggered(bool)), this, SLOT(loadFiles()));
	KAction *openDirectoryAction = setupKAction("document-open-folder", i18n("Open Directory..."), i18n("Load all files in the selected directory and its subdirectories"), "directory");
	openDirectoryAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
	connect(openDirectoryAction, SIGNAL(triggered(bool)), this, SLOT(loadDirectory()));
	KAction *previousAction = setupKAction("media-skip-backward", i18n("Previous"), previousHelpText, "previous");
	previousAction->setShortcut(QKeySequence(Qt::Key_Z));
	connect(previousAction, SIGNAL(triggered(bool)), this, SLOT(previous()));
	connect(tb_previousAction, SIGNAL(triggered(bool)), this, SLOT(previous()));
	KAction *pauseAction = setupKAction("media-playback-pause", i18n("Pause"), plauseHelpText, "pause");
	pauseAction->setShortcut(QKeySequence(Qt::Key_C));
	connect(pauseAction, SIGNAL(triggered(bool)), this, SLOT(pause()));
	connect(tb_pauseAction, SIGNAL(triggered(bool)), this, SLOT(pause()));
	KAction *playAction = setupKAction("media-playback-start", i18n("Play"), playHelpText, "play");
	playAction->setShortcut(QKeySequence(Qt::Key_X));
	connect(playAction, SIGNAL(triggered(bool)), this, SLOT(play()));
	connect(tb_playAction, SIGNAL(triggered(bool)), this, SLOT(play()));
	KAction *nextAction = setupKAction("media-skip-forward", i18n("Next"), nextHelpText, "next");
	nextAction->setShortcut(QKeySequence(Qt::Key_V));
	connect(nextAction, SIGNAL(triggered(bool)), this, SLOT(next()));
	connect(tb_nextAction, SIGNAL(triggered(bool)), this, SLOT(next()));
	KAction *queueAction = setupKAction("go-next-view", i18n("Queue Track"), "Enqueue the current track", "queue");
	queueAction->setShortcut(QKeySequence(Qt::Key_Q));
	connect(queueAction, SIGNAL(triggered(bool)), this, SLOT(queue()));
	shuffleAction = setupKAction("media-playlist-shuffle", i18n("Suffle"), "The next track will be random when checked", "shuffle");
	shuffleAction->setCheckable(true);
	connect(shuffleAction, SIGNAL(triggered(bool)), this, SLOT(shuffle(bool)));
	KAction *viewCurrentTrackAction = setupKAction("go-last", i18n("Current Track"), i18n("Show the Current Track in the Playlist"), "current_track");
	connect(viewCurrentTrackAction, SIGNAL(triggered(bool)), this, SLOT(viewCurrentTrack()));
	KAction *viewTrackDetailsAction = setupKAction("view-media-lyrics", i18n("Track Details"), i18n("View the playing track's metadata"), "track_details");
	connect(viewTrackDetailsAction, SIGNAL(triggered(bool)), this, SLOT(viewTrackDetails()));
	KAction *viewTrackQueueAction = setupKAction("view-time-schedule-edit", i18n("Track Queue"), i18n("Edit the track queue"), "track_queue");
	connect(viewTrackQueueAction, SIGNAL(triggered(bool)), this, SLOT(viewTrackQueue()));
	viewPlaylistAction = setupKAction("view-file-columns", i18n("Playlist"), i18n("Show/Hide the playlist"), "playlist");
	viewPlaylistAction->setCheckable(true);
	connect(viewPlaylistAction, SIGNAL(triggered(bool)), this, SLOT(viewPlaylist(bool)));
	connect(artist_list, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), this, SLOT(updateAlbumList(QListWidgetItem *,  QListWidgetItem *)));
	connect(album_list,  SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), this, SLOT(updateTitlesList(QListWidgetItem *, QListWidgetItem *)));
	connect(titles_list, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), this, SLOT(showTrackInfo(QListWidgetItem *,    QListWidgetItem *)));
	connect(titles_list, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(play(QListWidgetItem *)));
	connect(mw_ok_button,     SIGNAL(clicked()), this, SLOT(hideTrackDetails()));
	connect(qw_top_button,    SIGNAL(clicked()), this, SLOT(moveQueuedTrackToTop()));
	connect(qw_up_button,     SIGNAL(clicked()), this, SLOT(moveQueuedTrackUp()));
	connect(qw_down_button,   SIGNAL(clicked()), this, SLOT(moveQueuedTrackDown()));
	connect(qw_bottom_button, SIGNAL(clicked()), this, SLOT(moveQueuedTrackToBottom()));
	connect(qw_remove_button, SIGNAL(clicked()), this, SLOT(dequeueTrack()));
	connect(qw_ok_button,     SIGNAL(clicked()), this, SLOT(hideTrackQueue()));
	
	//SETUP GUI
	qsrand(QDateTime::currentDateTime().toTime_t());
	setupGUI(Default, "projekt7ui.rc");
	updateArtistList(0);
	
	//READ CONFIG
	config = KGlobal::config();
	KConfigGroup curTrackDetails(config, "curTrackDetails");
	cur_artist = artist_list->item(curTrackDetails.readEntry("artist", QString()).toInt());
	cur_album = curTrackDetails.readEntry("album", QString()).toInt();
	cur_title = curTrackDetails.readEntry("title", QString()).toInt();
	KConfigGroup applicationSettings(config, "applicationSettings");
	viewPlaylistAction->setChecked(applicationSettings.readEntry("playlistVisible", QString()).toInt());
	shuffle_tracks = applicationSettings.readEntry("shuffleTracks", QString()).toInt();
	shuffleAction->setChecked(shuffle_tracks);
	viewCurrentTrack();
	if (titles_list->count() > 0) {
		if (titles_list->currentRow() == -1)
			titles_list->setCurrentRow(0);
		play(titles_list->currentItem()->data(Qt::UserRole).toInt(), false, false);
		pause();
		quint64 song_position = curTrackDetails.readEntry("tick", QString()).toLongLong();
		now_playing->seek(song_position);
		tick(song_position);
		//TODO update the seekSlider's position to match the "tick" location of the song
	}
}

Player::~Player() {
	cleanup();
}

void Player::quit() {
	cleanup();
}

void Player::cleanup() {
	if (now_playing)
		now_playing->pause();
	delete queued;
	delete tray_icon;
	KConfigGroup curTrackDetails(config, "curTrackDetails");
	curTrackDetails.writeEntry("artist", QString::number(artist_list->row(cur_artist)));
	curTrackDetails.writeEntry("album",  QString::number(cur_album));
	curTrackDetails.writeEntry("title",  QString::number(cur_title));
	curTrackDetails.writeEntry("tick",   QString::number(now_playing->currentTime()));
	KConfigGroup applicationSettings(config, "applicationSettings");
	applicationSettings.writeEntry("playlistVisible", QString::number(viewPlaylistAction->isChecked()));
	applicationSettings.writeEntry("shuffleTracks",   QString::number(shuffleAction->isChecked()));
	sqlite3_close(tracks_db);
}

KAction* Player::setupKAction(const char *icon, QString text, QString help_text, const char *name) {
	KAction* action = new KAction(this);
	action->setIcon(KIcon(icon));
	action->setText(text);
	action->setHelpText(help_text);
	actionCollection()->addAction(name, action);
	return action;
}

void Player::loadFiles() {
	loadFiles(KFileDialog::getOpenFileNames(KUrl(), "audio/mpeg audio/mp4 audio/ogg audio/aac audio/flac")); //TODO replace the explicit type list with a generic audio list (does not see *.mp4 audio files)
}

void Player::loadDirectory() {
	QStringList files;
	QString path = KFileDialog::getExistingDirectory(); //TODO filter for only audio files
	if (path == "")
		return;
	readDirectory(path, files);
	loadFiles(files);
}

void Player::readDirectory(const QDir &dir, QStringList &files) {
	QFileInfoList children = dir.entryInfoList();
	QFileInfoList::iterator itt, end = children.end();
	for (itt = children.begin(); itt != end; ++itt) {
		QString name = itt->fileName();
		if (name != "." && name != "..") {
			if (itt->isDir())
				readDirectory(QDir(itt->absoluteFilePath()), files);
			else
				files.push_back(itt->absoluteFilePath());
		}
	}
}

void Player::loadFiles(const QStringList &files) {
	if (files.count() == 0)
		return;
	QProgressDialog progress("    Don't worry. I'm wondering why it takes so long to read tag information too ...    ", "Cancel", 2, files.count(), this);
	progress.setWindowModality(Qt::WindowModal);
	uint i = 0;
	QStringList::const_iterator itt, end = files.constEnd();
	for (itt = files.constBegin(); itt != end; ++itt) {
		progress.setValue(++i);
		TagLib::FileRef f(itt->toUtf8().constData()); //NOTE: don't ask me why TabLib won't accept qtos(*itt), but this seems to work for international characters
		char *query = sqlite3_mprintf("INSERT INTO `tracks` (`artist`, `year`, `album`, `track_number`, `title`, `path`) VALUES (%Q, %u, %Q, %u, %Q, %Q)", f.tag()->artist().toCString(), f.tag()->year(), f.tag()->album().toCString(), f.tag()->track(), f.tag()->title().toCString(), qtos(*itt));
		char *errmsg;
		int return_code = sqlite3_exec(tracks_db, query, 0, 0, &errmsg);
		if (return_code) {
			showError("Failed to insert tracks: ", errmsg);
			sqlite3_free(errmsg);
			exit(return_code);
		}
		sqlite3_free(query);
		if (progress.wasCanceled())
			break;
	}
	updateNumTracks();
	updateArtistList(cur_artist);
}

void Player::enqueueNext() {
	next(false);
}

void Player::previous() {
	bool skipped_to_get_here = false;
	if (history.count() > 1) {
		bool track_exists = false;
		bool first_attempt = true;
		do {
			if (first_attempt) {
				history.pop_back();
				first_attempt = false;
			}
			HistoryItem prev = history.takeLast();
			char *query = sqlite3_mprintf("SELECT `tid` FROM `tracks` WHERE `tid`=%u", prev.tid);
			sqlite3_stmt *trackQuery = 0;
			prepare(query, &trackQuery, "Failed to Prepare `tid` query: ");
			bool done = false;
			do {
				if (step(trackQuery, done, true, "Failed to Step `tid` in previous: "))
					track_exists = true;
				else
					skipped_to_get_here = true;
			} while (!done);
			if (track_exists) {
				cur_artist = prev.artist;
				cur_album = prev.album;
				cur_title = prev.title;
				viewCurrentTrack();
				play(prev.tid);
			}
		} while (!track_exists && history.count() > 0);
	}
	if (history.count() <= 1 && !skipped_to_get_here) {
		if (history.count() == 1)
			history.pop_back();
		now_playing->stop();
		setWindowTitle("Projekt 7");
	}
}

void Player::play() {
	if (now_playing->state() == Phonon::PausedState) //TODO: figure out why this broke
		now_playing->play();
	else if (titles_list->currentItem())
		play(titles_list->currentItem()->data(Qt::UserRole).toInt());
}

void Player::play(QListWidgetItem *titles_list_item) {
	if (titles_list_item != 0)
		play(titles_list_item->data(Qt::UserRole).toInt());
}

void Player::play(int tid, bool play, bool add_to_history) {
	cur_artist = artist_list->currentItem();
	cur_album = album_list->currentRow() > 0 ? album_list->currentRow() : 0;
	cur_title = titles_list->currentRow() > 0 ? titles_list->currentRow() : 0;
	if (add_to_history) {
		history.push_back(HistoryItem(cur_artist, cur_album, cur_title, tid));
		if (history.count() > 100)
			history.pop_front();
	}
	sqlite3_stmt *trackQuery = 0;
	char *query = sqlite3_mprintf("SELECT `artist`, `year`, `album`, `track_number`, `title`, `path` FROM `tracks` WHERE `tid`=%u LIMIT 1", tid);
	prepare(query, &trackQuery, "Failed to Prepare `path` query: ");
	bool done = false;
	do {
		if (step(trackQuery, done, true, "Failed to Step `path` in play: ")) {
			char *path = sqlite3_mprintf("%s", sqlite3_column_text(trackQuery, 5)); //NOTE: why does sqlite3_column_text return an `unsigned char *`?  who uses that?!
			QString qpath(path);
			sqlite3_free(path);
			if (play) {
				now_playing->setCurrentSource(qpath);
				now_playing->play();
				tick(0);
			}
			else
				now_playing->enqueue(qpath);
			setQLabelText("%s", trackQuery, 0, mw_artist);
			setQLabelText("%s", trackQuery, 1, mw_year);
			setQLabelText("%s", trackQuery, 2, mw_album);
			setQLabelText("%s", trackQuery, 3, mw_track_number);
			setQLabelText("%s", trackQuery, 4, mw_title);
			mw_path->setText(qpath);
			setWindowTitle(mw_artist->text() + " - " + mw_title->text() + "  |  Projekt 7");
			if (add_to_history)
				tray_icon->showMessage("Projekt 7 | Now Playing:", mw_artist->text() + " - " + mw_title->text(), QSystemTrayIcon::NoIcon, 5000);
		}
	} while (!done);
}

void Player::pause() {
	now_playing->pause();
}

void Player::next() {
	next(true);
}

void Player::next(bool play_track) {
	if (titles_list->count() == 0)
		return;
	if (track_queue.count()) {
		int tid = track_queue.takeFirst();
		selectTrack(tid);
		track_queue_info.remove(tid);
		titles_list->currentItem()->setIcon(dequeud);
	} else {
		if (shuffle_tracks) {
			char *query = sqlite3_mprintf("SELECT `tid` FROM `tracks` LIMIT 1 OFFSET %i", (qrand() % num_tracks) + 1); //NOTE: the lowest `tid` is '1'
			sqlite3_stmt *shuffleQuery = 0;
			prepare(query, &shuffleQuery, "Failed to Prepare shuffle query: ");
			bool done = false;
			int tid = 0;
			if (step(shuffleQuery, done, true, "Failed to Step shuffle in next(): "))
				tid = sqlite3_column_int(shuffleQuery, 0);
			selectTrack(tid);
		} else {
			artist_list->setCurrentItem(cur_artist);
			if (++cur_title >= titles_list->count()) {
				cur_title = 0;
				if (cur_album == 0 || ++cur_album >= album_list->count()) {
					cur_album = 1;
					int next_artist = artist_list->row(cur_artist) + 1;
					cur_artist = artist_list->item(next_artist >= artist_list->count() ? 1 : next_artist);
					artist_list->setCurrentItem(cur_artist);
				}
				else
					album_list->setCurrentItem(album_list->item(cur_album));
			}
			titles_list->setCurrentItem(titles_list->item(cur_title));
		}
	}
	play(titles_list->currentItem()->data(Qt::UserRole).toInt(), play_track);
}

void Player::queue() {
	int tid = titles_list->currentItem()->data(Qt::UserRole).toInt();
	if (track_queue.contains(tid)) {
		track_queue.removeAll(tid);
		track_queue_info.remove(tid);
		titles_list->currentItem()->setIcon(dequeud);
	} else {
		track_queue.push_back(tid);
		track_queue_info.insert(tid, artist_list->currentItem()->text() + " - " + titles_list->currentItem()->text());
		titles_list->currentItem()->setIcon(*queued);
	}
}

void Player::shuffle(bool checked) {
	shuffle_tracks = checked;
}

void Player::updateDuration(qint64 duration) {
	QString time;
	QTextStream qout(&time);
	qout << formatTime(duration);
	track_duration->setText(time);
}

void Player::tick(qint64 tick) {
	QString time;
	QTextStream qout(&time);
	qout << ' ' << formatTime(tick);
	cur_time->setText(time);
}

void Player::viewCurrentTrack() {
	if (cur_artist)
		artist_list->setCurrentItem(cur_artist);
	else
		artist_list->setCurrentRow(0);
	if (album_list->currentRow() != cur_album)
		album_list->setCurrentItem(album_list->item(cur_album));
	titles_list->setCurrentRow(cur_title);
}

void Player::viewTrackDetails() {
	metadata_window->setVisible(true);
}

void Player::hideTrackDetails() {
	metadata_window->setVisible(false);
}

void Player::viewTrackQueue() {
	int tid;
	foreach(tid, track_queue)
		qw_queue_list->addItem(new QListWidgetItem(track_queue_info[tid]));
	queue_window->setVisible(true);
}

void Player::hideTrackQueue() {
	qw_queue_list->clear();
	updateTitlesList(album_list->currentItem());
	queue_window->setVisible(false);
}

void Player::viewPlaylist(bool show) {
	if (show) {
		playlist_widget->setVisible(true);
		statusBar()->setVisible(true);
	} else { //TODO: fix the geometry issue of it remembering the minimum space required to show the playlist
		playlist_widget->setVisible(false);
		statusBar()->setVisible(false);
		resize(minimumWidth(), minimumHeight());
	}
}

void Player::moveQueuedTrackToTop() {
	int row = qw_queue_list->currentRow();
	if (row == 0)
		return;
	track_queue.push_front(track_queue.takeAt(row));
	qw_queue_list->insertItem(0, qw_queue_list->takeItem(row));
	qw_queue_list->setCurrentRow(0);
}

void Player::moveQueuedTrackUp() {
	int row = qw_queue_list->currentRow();
	if (row == 0)
		return;
	track_queue.swap(row - 1, row);
	qw_queue_list->insertItem(row - 1, qw_queue_list->takeItem(row));
	qw_queue_list->setCurrentRow(row - 1);
}

void Player::moveQueuedTrackDown() {
	int row = qw_queue_list->currentRow();
	if (row == qw_queue_list->count() - 1)
		return;
	qw_queue_list->insertItem(row + 1, qw_queue_list->takeItem(row));
	qw_queue_list->setCurrentRow(row + 1);
}

void Player::moveQueuedTrackToBottom() {
	int row = qw_queue_list->currentRow();
	if (row == qw_queue_list->count() - 1)
		return;
	qw_queue_list->addItem(qw_queue_list->takeItem(row));
	qw_queue_list->setCurrentRow(qw_queue_list->count() - 1);
}

void Player::dequeueTrack() {
	int row = qw_queue_list->currentRow();
	track_queue.removeAt(row);
	track_queue_info.remove(qw_queue_list->currentItem()->data(Qt::UserRole).toInt());
	delete qw_queue_list->takeItem(row);
}

void Player::updateArtistList(QListWidgetItem *artist_list_item) {
	static sqlite3_stmt *artistQuery = 0;
	if (artistQuery == 0) {
		char *query = sqlite3_mprintf("%s", "SELECT `artist` FROM `tracks` GROUP BY `artist` COLLATE NOCASE");
		prepare(query, &artistQuery, "Failed to Prepare `artist` GUI update query: ");
	}
	bool done = false;
	bool artists_present = artist_list->count() > 1;
	int i = 0;
	do {
		if (step(artistQuery, done, false, "Failed to Step `artist` in GUI update: ")) {
			char *artist = sqlite3_mprintf("%s", sqlite3_column_text(artistQuery, 0)); //NOTE: why does sqlite3_column_text return an `unsigned char *`?  who uses that?!
			if (artists_present && i < artist_list->count() - 1) {
				QString qartist(artist);
				//NOTE: if the location in the list of artists is not equal to the value from the database, then
				//this artist was added during the most recent call to `loadFiles` or `readDirectory` and needs to be added to the list
				if (artist_list->item(++i)->text() != qartist) {
					artist_list->insertItem(i, qartist);
				}
			}
			else
				artist_list->addItem(artist);
			sqlite3_free(artist);
		}
	} while (!done);
	if (artist_list_item == 0) {
		if (artist_list->count() > 0)
			artist_list->setCurrentItem(0);
	}
	else
		artist_list->setCurrentItem(artist_list_item);
}

void Player::updateAlbumList(QListWidgetItem *artist_list_item, QListWidgetItem *prev_artist) {
	if (artist_list_item == prev_artist)
		return;
	sqlite3_stmt *albumQuery;
	char *query;
	if (artist_list_item == 0 || artist_list->currentRow() == 0)
		query = sqlite3_mprintf("%s", "SELECT `artist`, `album` FROM `tracks` GROUP BY `album` ORDER BY `album` COLLATE NOCASE");
	else
		query = sqlite3_mprintf("SELECT `artist`, `album` FROM `tracks` WHERE `artist`=%Q GROUP BY `album` ORDER BY `year`, `album` COLLATE NOCASE", qtos(artist_list_item->text()));
	prepare(query, &albumQuery, "Failed to Prepare `album` GUI update query: ");
	bool done = false;
	album_list->clear();
	album_list->addItem(ALL);
	do {
		if (step(albumQuery, done, true, "Failed to Step `album` in GUI update: ")) {
			char *artist = sqlite3_mprintf("%s", sqlite3_column_text(albumQuery, 0)); //NOTE: why does sqlite3_column_text return an `unsigned char *`?  who uses that?!
			char *album = sqlite3_mprintf("%s", sqlite3_column_text(albumQuery, 1)); //NOTE: why does sqlite3_column_text return an `unsigned char *`?  who uses that?!
			QListWidgetItem *new_item = new QListWidgetItem(album);
			new_item->setData(Qt::UserRole, artist);
			album_list->addItem(new_item);
			sqlite3_free(artist);
			sqlite3_free(album);
		}
	} while (!done);
	if (artist_list->currentItem() != artist_list_item)
		artist_list->setCurrentRow(artist_list->row(artist_list_item));
	if (album_list->count() > 0) {
		if (artist_list_item == cur_artist)
			album_list->setCurrentItem(album_list->item(cur_album));
		else
			album_list->setCurrentItem(album_list->item(0));
	}
}

void Player::updateTitlesList(QListWidgetItem *album_list_item, QListWidgetItem *prev_album) {
	if (album_list_item == prev_album)
		return;
	sqlite3_stmt *titleQuery;
	bool all_albums = album_list_item == 0 || album_list->currentRow() == 0;
	bool all_artists = artist_list->currentRow() == 0 || artist_list->currentItem() == 0;
	char *query;
	if (all_artists) {
		if (all_albums)
			query = sqlite3_mprintf("%s", "SELECT `tid`, `title` FROM `tracks` ORDER BY `title` COLLATE NOCASE");
		else
			query = sqlite3_mprintf("SELECT `tid`, `track_number`, `title` FROM `tracks` WHERE `album`=%Q ORDER BY `track_number`", qtos(album_list_item->text()));
	} else {
		if (all_albums)
			query = sqlite3_mprintf("SELECT `tid`, `title` FROM `tracks` WHERE `artist`=%Q ORDER BY `title` COLLATE NOCASE", qtos(artist_list->currentItem()->text()));
		else
			query = sqlite3_mprintf("SELECT `tid`, `track_number`, `title` FROM `tracks` WHERE `artist`=%Q AND `album`=%Q ORDER BY `track_number`", qtos(album_list_item->data(Qt::UserRole).toString()), qtos(album_list_item->text()));
	}
	prepare(query, &titleQuery, "Failed to Prepare `title` GUI update query: ");
	bool done = false;
	titles_list->clear();
	do {
		if (step(titleQuery, done, true, "Failed to Step `title` in GUI update: ")) {
			int tid = sqlite3_column_int(titleQuery, 0);
			char *title;
			if (all_albums)
				title = sqlite3_mprintf("%s", sqlite3_column_text(titleQuery, 1)); //NOTE: why does sqlite3_column_text return an `unsigned char *`?  who uses that?!
			else
				title = sqlite3_mprintf("%d. %s", sqlite3_column_int(titleQuery, 1), sqlite3_column_text(titleQuery, 2)); //NOTE: why does sqlite3_column_text return an `unsigned char *`?  who uses that?!
			QListWidgetItem *new_item = new QListWidgetItem(title);
			new_item->setData(Qt::UserRole, tid);
			if (track_queue.contains(tid))
				new_item->setIcon(*queued);
			titles_list->addItem(new_item);
			sqlite3_free(title);
		}
	} while (!done);
	if (cur_artist == artist_list->currentItem() && album_list_item == album_list->item(cur_album))
		titles_list->setCurrentRow(cur_title);
	else
		titles_list->setCurrentRow(0);
}

void Player::showTrackInfo(QListWidgetItem *titles_list_item, QListWidgetItem *) {
	if (titles_list_item == 0)
		return;
	sqlite3_stmt *trackQuery = 0;
	char *query = sqlite3_mprintf("SELECT `artist`, `year`, `album`, `track_number`, `title` FROM `tracks` WHERE `tid`=%u LIMIT 1", titles_list_item->data(Qt::UserRole).toInt());
	prepare(query, &trackQuery, "Failed to Prepare status bar update query: ");
	bool done = false;
	do {
		if (step(trackQuery, done, true, "Failed to Step in status bar update: ")) {
			char *track_text = sqlite3_mprintf("%s - %u - %s - %u - %s", sqlite3_column_text(trackQuery, 0), sqlite3_column_int(trackQuery, 1), sqlite3_column_text(trackQuery, 2), sqlite3_column_int(trackQuery, 3), sqlite3_column_text(trackQuery, 4));
			statusBar()->changeItem(track_text, SONG_NAME);
			sqlite3_free(track_text);
		}
	} while (!done);
}

void Player::keyReleaseEvent(QKeyEvent *event) {
	switch(event->key()) {
		case Qt::Key_Delete:
		case Qt::Key_Backspace: {
			enum { AllTracksLevel, ArtistLevel, AlbumLevel, TrackLevel } delete_level = TrackLevel;
			if (artist_list->hasFocus()) {
				if (artist_list->currentRow() == 0)
					delete_level = AllTracksLevel;
				else
					delete_level = ArtistLevel;
			} else if (album_list->hasFocus()) {
				if (album_list->currentRow() == 0 || album_list->count() == 2)
					delete_level = ArtistLevel;
				else
					delete_level = AlbumLevel;
			} else if (titles_list->hasFocus()) {
				if (titles_list->count() == 1) {
					if (album_list->count() == 2)
						delete_level = ArtistLevel;
					else
						delete_level = AlbumLevel;
				}
				else
					delete_level = TrackLevel;
			}
			sqlite3_stmt *deleteQuery;
			char *query;
			switch (delete_level) {
				case AllTracksLevel: query = sqlite3_mprintf("%s", "DELETE FROM `tracks`"); break;
				case ArtistLevel:    query = sqlite3_mprintf("DELETE FROM `tracks` WHERE `artist`=%Q", qtos(artist_list->currentItem()->text())); break;
				case AlbumLevel:     query = sqlite3_mprintf("DELETE FROM `tracks` WHERE `artist`=%Q AND `album`=%Q", qtos(artist_list->currentItem()->text()), qtos(album_list->currentItem()->text())); break;
				case TrackLevel:     query = sqlite3_mprintf("DELETE FROM `tracks` WHERE `tid`=%d", titles_list->currentItem()->data(Qt::UserRole).toInt()); break;
			}
			prepare(query, &deleteQuery, "Failed to Prepare DELETE query: ");
			bool done = false; //NOTE: value ignored for the case of a DELETE query, but required for the use of the the `step` convenience function
			step(deleteQuery, done, true, "Failed to Step DELETE: ");
			switch (delete_level) {
				case AllTracksLevel:
					artist_list->clear();
					artist_list->addItem(ALL);
					cur_artist = artist_list->item(0);
					album_list->clear();
					cur_album = 0;
					titles_list->clear();
					cur_title = 0;
					statusBar()->changeItem("", SONG_NAME);
					artist_list->setCurrentItem(artist_list->item(0));
					break;
				case ArtistLevel: {
					int row = artist_list->currentRow();
					bool update_cur_artist = cur_artist == artist_list->currentItem();
					delete artist_list->takeItem(row);
					row = row == artist_list->count() ? artist_list->count() - 1 : row;
					if (update_cur_artist) {
						cur_artist = artist_list->item(row);
						cur_album = 1;
						cur_title = 0;
					}
					artist_list->setCurrentItem(artist_list->item(row), 0);
					break;
				}
				case AlbumLevel: {
					int row = album_list->currentRow();
					bool update_cur_album = cur_artist == artist_list->currentItem() && cur_album == row;
					updateAlbumList(artist_list->currentItem());
					row = row == album_list->count() ? album_list->count() - 1 : row;
					if (update_cur_album) {
						cur_album = row;
						cur_title = 0;
					}
					album_list->setCurrentRow(row);
					break;
				}
				case TrackLevel: {
					int row = titles_list->currentRow();
					bool update_cur_title = cur_artist == artist_list->currentItem() && cur_album == album_list->currentRow() && cur_title == row;
					updateTitlesList(album_list->currentItem());
					row = row == titles_list->count() ? titles_list->count() - 1 : row;
					if (update_cur_title)
						cur_title = row;
					titles_list->setCurrentRow(row);
					break;
				}
			}
			break;
		}
		default: break;
	}
}

void Player::prepare(char *query, sqlite3_stmt **stmt, const char *failure_msg) {
	int return_code = sqlite3_prepare_v2(tracks_db, query, -1, stmt, 0);
	if (return_code) {
		showError(failure_msg, sqlite3_errmsg(tracks_db));
		exit(return_code);
	}
	sqlite3_free(query);
}

bool Player::step(sqlite3_stmt *stmt, bool &done, bool finalize, const char *failure_msg) {
	int return_code = sqlite3_step(stmt);
	switch (return_code) {
		case SQLITE_ROW:
			return true;
		case SQLITE_DONE:
			done = true;
			if (finalize)
				sqlite3_finalize(stmt);
			else
				sqlite3_reset(stmt);
			return false;
		default:
			done = true;
			if (finalize)
				sqlite3_finalize(stmt);
			else
				sqlite3_reset(stmt);
			showError(failure_msg, sqlite3_errmsg(tracks_db));
			exit(return_code);
			return false;
	}
}

void Player::showError(QString part1, QString part2) {
	KMessageBox::error(this, part1 + part2);
}

void Player::setQLabelText(const char *format, sqlite3_stmt *stmt, int index, QLabel *label) {
	char *text = sqlite3_mprintf(format, sqlite3_column_text(stmt, index));
	label->setText(text);
	sqlite3_free(text);
}

void Player::selectTrack(int tid) {
	char *query = sqlite3_mprintf("SELECT `artist`, `album`, `title` FROM `tracks` WHERE `tid`=%u", tid);
	sqlite3_stmt *selectQuery = 0;
	prepare(query, &selectQuery, "Failed to Prepare selectTrack query: ");
	bool done = false;
	if (step(selectQuery, done, true, "Failed to Step select in selectTrack: ")) {
		char *artist_name = sqlite3_mprintf("%s", sqlite3_column_text(selectQuery, 0)); //NOTE: why does sqlite3_column_text return an `unsigned char *`?  who uses that?!
		QListWidgetItem *artist = artist_list->findItems(artist_name, Qt::MatchCaseSensitive).takeFirst();
		sqlite3_free(artist_name);
		artist_list->setCurrentItem(artist);
		char *album_name = sqlite3_mprintf("%s", sqlite3_column_text(selectQuery, 1)); //NOTE: why does sqlite3_column_text return an `unsigned char *`?  who uses that?!
		QListWidgetItem *album = album_list->findItems(album_name, Qt::MatchCaseSensitive).takeFirst();
		sqlite3_free(album_name);
		album_list->setCurrentItem(album);
		char *track_name = sqlite3_mprintf("%s", sqlite3_column_text(selectQuery, 2)); //NOTE: why does sqlite3_column_text return an `unsigned char *`?  who uses that?!
		QList<QListWidgetItem *> tracks = titles_list->findItems(track_name, Qt::MatchEndsWith);
		sqlite3_free(track_name);
		QListWidgetItem *item, *title = 0;
		foreach(item, tracks) { //NOTE: loop through the found items to prevent the case of selecting the wrong track with the same name
			if (item->data(Qt::UserRole).toInt() == tid) {
				title = item;
				break;
			}
		}
		titles_list->setCurrentItem(title);
	}
}

void Player::updateNumTracks() {
	char *query = sqlite3_mprintf("%s", "SELECT count(*) FROM `tracks`");
	sqlite3_stmt *countQuery = 0;
	prepare(query, &countQuery, "Failed to Prepare `count(*)` query: ");
	bool done = false;
	if (step(countQuery, done, true, "Failed to Step `count(*)` in constructor: "))
		num_tracks = sqlite3_column_int(countQuery, 0);
	else
		num_tracks = 0;
}
