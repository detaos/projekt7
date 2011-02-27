#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <QDir>
#include <QHash>
#include <QLabel>
#include <QList>
#include <QLinkedList>
#include <QListWidgetItem>
#include <QStringList>
#include <QWidget>

#include <KAction>
#include <KConfig>
#include <KListWidget>
#include <KPushButton>
#include <KSystemTrayIcon>
#include <KXmlGuiWindow>

#include <phonon/mediaobject.h>

#include <sqlite3.h>

struct HistoryItem {
	HistoryItem(QListWidgetItem *q, int a, int t, int i) : artist(q), album(a), title(t), tid(i) {};
	QListWidgetItem *artist;
	int album, title, tid;
};

class Player : public KXmlGuiWindow
{
	Q_OBJECT 
	
	public:
		Player(QWidget *parent = 0);
		~Player();
		
	private slots:
		void quit();
		
		void loadFiles();
		void loadDirectory();
		
		void enqueueNext();
		
		void previous();
		void pause();
		void play();
		void play(QListWidgetItem *);
		void next();
		void queue();
		void shuffle(bool);
		void updateDuration(qint64);
		void tick(qint64);
		
		void updateArtistList(QListWidgetItem *);
		void updateAlbumList(QListWidgetItem *, QListWidgetItem * = 0);
		void updateTitlesList(QListWidgetItem *, QListWidgetItem * = 0);
		void showTrackInfo(QListWidgetItem *, QListWidgetItem * = 0);
		
		void viewCurrentTrack();
		void viewTrackDetails();
		void hideTrackDetails();
		void viewTrackQueue();
		void hideTrackQueue();
		void viewPlaylist(bool);
		
		void moveQueuedTrackToTop();
		void moveQueuedTrackUp();
		void moveQueuedTrackDown();
		void moveQueuedTrackToBottom();
		void dequeueTrack();
		
	protected:
		void keyReleaseEvent(QKeyEvent *);
		
	private:
		void cleanup();
		inline KAction* setupKAction(const char *, QString, QString, const char *);
		void readDirectory(const QDir &, QStringList &);
		void loadFiles(const QStringList &);
		void next(bool);
		void play(int, bool = true, bool = true);
		inline void prepare(char *, sqlite3_stmt **, const char *);
		inline bool step(sqlite3_stmt *, bool &, bool, const char *);
		inline void showError(QString, QString);
		inline void setQLabelText(const char *, sqlite3_stmt *, int, QLabel *);
		void selectTrack(int);
		void updateNumTracks();
		
		sqlite3 *tracks_db;
		KSharedConfigPtr config;
		QWidget *playlist_widget, *metadata_window, *queue_window;
		KAction *shuffleAction, *viewPlaylistAction;
		QLabel *mw_artist, *mw_year, *mw_album, *mw_track_number, *mw_title, *mw_path;
		KPushButton *mw_ok_button, *qw_ok_button;
		QLabel *cur_time, *track_duration;
		KListWidget *artist_list, *album_list, *titles_list, *qw_queue_list;
		QListWidgetItem *cur_artist;
		int cur_album, cur_title, num_tracks;
		bool shuffle_tracks;
		Phonon::MediaObject *now_playing;
		QList<int> track_queue; //a queue ... push_back to add ... takeFirst to retrieve
		QHash<int, QString> track_queue_info;
		KIcon *queued, dequeud;
		QLinkedList<HistoryItem> history; //a stack ... push_back to add ... takeLast to retrieve next
		KSystemTrayIcon *tray_icon;
};

#endif
