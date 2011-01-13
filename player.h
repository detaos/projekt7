#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <QtGui>
#include <QtSql>
#include <QWidget>

#include <KListWidget>
#include <KXmlGuiWindow>

#include <phonon/mediaobject.h>

class Player : public KXmlGuiWindow
{
	Q_OBJECT 
	
	public:
		Player(QWidget *parent=0);
		~Player();
		
	private slots:
		void loadFiles();
		
		void enqueueNext();
		
		void previous();
		void play();
		void pause();
		void next();
		
		void viewTrackDetails();
		
	private:
		QWidget *central_widget;
		KListWidget *artist_list, *album_list, *titles_list;
		Phonon::MediaObject *now_playing;
};

#endif
