#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>

#include "player.h"

int main(int argc, char* argv[]) {
	KAboutData aboutData("projekt7", "projekt7",
						 ki18n("Projekt 7"), "0.9.9",
						 ki18n("The best damn music player ever made!"),
						 KAboutData::License_GPL_V3,
						 ki18n("Copyright (c) 2011 Rick Battle <rick.battle@solmera.com>"));
	
	KCmdLineArgs::init(argc, argv, &aboutData);
	KApplication app;
	Player* player = new Player();
	player->show();
	return app.exec();
}
