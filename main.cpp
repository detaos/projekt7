#include <cstdio>

#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>

#include "player.h"

int main(int argc, char* argv[]) {
	KAboutData aboutData("projekt7", "projekt7",
		ki18n("Projekt 7"), "0.1.2",
		ki18n("The best damn music player ever made!"),
		KAboutData::License_GPL,
		ki18n("Copyright (c) 2010 Rick Battle <rick.battle@celtrenicdesigns.com>"));
		
	KCmdLineArgs::init(argc, argv, &aboutData);
	KApplication app;
	Player* player = new Player();
	player->show();
	return app.exec();
}
