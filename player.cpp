#include "player.h"

#include <KAction>
#include <KActionCollection>
#include <KApplication>
#include <KLocale>
#include <KStandardAction>

#include <phonon/mediaobject.h>
#include <phonon/mediasource.h>
#include <phonon/path.h>
#include <phonon/audiooutput.h>

Player::Player(QWidget *parent) : KXmlGuiWindow(parent) {
	//SETUP DATABASE
	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "projekt7");
	if (!db.tables().contains("tracks")) {
		//TODO create the table
	}
	//TODO fisnish setting up the database
	
	//SETUP PHONON
	now_playing = new Phonon::MediaObject(this);
	createPath(now_playing, new Phonon::AudioOutput(Phonon::MusicCategory, this));
	
	//SETUP WIDGETS
	central_widget = new QWidget(this);
	artist_list = new KListWidget(central_widget);
	artist_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	album_list = new KListWidget(central_widget);
	album_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	titles_list = new KListWidget(central_widget);
	titles_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	
	QHBoxLayout *listLayout = new QHBoxLayout;
	listLayout->addWidget(artist_list);
	listLayout->addWidget(album_list);
	listLayout->addWidget(titles_list);
	central_widget->setLayout(listLayout);
	setCentralWidget(central_widget);
	
	//SETUP ACTIONS
	KStandardAction::quit(kapp, SLOT(quit()), actionCollection());
	KStandardAction::open(this, SLOT(loadFiles()), actionCollection());
	
	connect(now_playing, SIGNAL(aboutToFinish()), this, SLOT(enqueueNext()));
	
	KAction* previousAction = new KAction(this);
	previousAction->setIcon(KIcon("media-skip-backward"));
	previousAction->setText(i18n("Previous"));
	previousAction->setHelpText(i18n("Previous"));
	actionCollection()->addAction("previous", previousAction);
	connect(previousAction, SIGNAL(triggered(bool)), this, SLOT(previous()));
	
	KAction* playAction = new KAction(this);
	playAction->setIcon(KIcon("media-playback-start"));
	playAction->setText(i18n("Play"));
	playAction->setHelpText(i18n("Play"));
	actionCollection()->addAction("play", playAction);
	connect(playAction, SIGNAL(triggered(bool)), this, SLOT(play()));
	
	KAction* pauseAction = new KAction(this);
	pauseAction->setIcon(KIcon("media-playback-pause"));
	pauseAction->setText(i18n("Pause"));
	pauseAction->setHelpText(i18n("Pause"));
	actionCollection()->addAction("pause", pauseAction);
	connect(pauseAction, SIGNAL(triggered(bool)), this, SLOT(pause()));
	
	KAction* nextAction = new KAction(this);
	nextAction->setIcon(KIcon("media-skip-forward"));
	nextAction->setText(i18n("Next"));
	nextAction->setHelpText(i18n("Next"));
	actionCollection()->addAction("next", nextAction);
	connect(nextAction, SIGNAL(triggered(bool)), this, SLOT(next()));
	
	KAction* viewTrackDetailsAction = new KAction(this);
	viewTrackDetailsAction->setIcon(KIcon("view-media-lyrics"));
	viewTrackDetailsAction->setText(i18n("Track Details"));
	viewTrackDetailsAction->setHelpText(i18n("View the track's meta data"));
	actionCollection()->addAction("track_details", viewTrackDetailsAction);
	connect(viewTrackDetailsAction, SIGNAL(triggered(bool)), this, SLOT(viewTrackDetails()));
	
	setupGUI(Default, "projekt7ui.rc");
}

Player::~Player() {
	if (now_playing)
		now_playing->stop();
}

void Player::loadFiles() {
	//TODO load files using taglib to read the tags and store the info in the sqlite database
}

void Player::enqueueNext() {
	//TODO figure out what the next track is and queue it
}

void Player::previous() {
	//TODO implement a history
}

void Player::play() {
	//TODO play the track
}

void Player::pause() {
	//TODO pause the track
}

void Player::next() {
	//TODO figure out what the next track is and play it
}

void Player::viewTrackDetails() {
	//TODO show a dialog with all of the track's meta data
}
