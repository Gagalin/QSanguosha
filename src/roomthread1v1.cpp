#include "roomthread1v1.h"
#include "room.h"
#include "engine.h"
#include "settings.h"

#include <QDateTime>

RoomThread1v1::RoomThread1v1(Room *room)
    :QThread(room), room(room)
{

}

void RoomThread1v1::run(){
    // initialize the random seed for this thread
    qsrand(QTime(0,0,0).secsTo(QTime::currentTime()));

    QSet<QString> banset = Config.value("1v1/Banlist").toStringList().toSet();
    general_names = Sanguosha->getRandomGenerals(10, banset);

    QStringList known_list = general_names.mid(0, 6);
    unknown_list = general_names.mid(6, 4);

    int i;
    for(i=0; i<4; i++){
        general_names[i + 6] = QString("x%1").arg(i);
    }

    QString unknown_str = "+x0+x1+x2+x3";

    room->broadcastInvoke("fillGenerals", known_list.join("+") + unknown_str);

    ServerPlayer *first = room->players.at(0), *next = room->players.at(1);
    askForTakeGeneral(first);

    while(general_names.length() > 1){
        qSwap(first, next);

        askForTakeGeneral(first);
        askForTakeGeneral(first);
    }

    askForTakeGeneral(next);

    startArrange(first);
    startArrange(next);

    room->sem->acquire(2);
}

void RoomThread1v1::askForTakeGeneral(ServerPlayer *player){
    QString name;
    if(general_names.length() == 1)
        name = general_names.first();
    else if(player->getState() != "online"){
        int r = qrand() % general_names.length();
        name = general_names.at(r);
    }

    if(name.isNull()){
        player->invoke("askForGeneral1v1");
    }else{
        msleep(1000);
        takeGeneral(player, name);
    }

    room->sem->acquire();
}

void RoomThread1v1::takeGeneral(ServerPlayer *player, const QString &name){
    QString group = player->isLord() ? "warm" : "cool";
    room->broadcastInvoke("takeGeneral", QString("%1:%2").arg(group).arg(name), player);

    QRegExp unknown_rx("x(\\d)");
    QString general_name = name;
    if(unknown_rx.exactMatch(name)){
        int index = unknown_rx.capturedTexts().at(1).toInt();
        general_name = unknown_list.at(index);

        player->invoke("recoverGeneral", QString("%1:%2").arg(index).arg(general_name));
    }

    player->invoke("takeGeneral", QString("%1:%2").arg(group).arg(general_name));

    general_names.removeOne(name);
    player->addToSelected(general_name);

    room->sem->release();
}

void RoomThread1v1::startArrange(ServerPlayer *player){
    if(player->getState() != "online"){
        QStringList arranged = player->getSelected();
        qShuffle(arranged);
        arranged = arranged.mid(0, 3);
        arrange(player, arranged);
    }else{
        player->invoke("startArrange");
    }
}

void RoomThread1v1::arrange(ServerPlayer *player, const QStringList &arranged){
    Q_ASSERT(arranged.length() == 3);

    QStringList left = arranged.mid(1, 2);
    player->tag["1v1Arrange"] = QVariant::fromValue(left);
    player->setGeneralName(arranged.first());

    room->sem->release();
}

