#include "mainwindow.h"
#include "optics.h"
#include <QtWidgets>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	mainfield = new field(this);
	setCentralWidget(mainfield);

	setMinimumSize(700,700);

	for (int i = -3; i < 4; i++)
		new ray(0.50, 0.15, 4 + i * 0.05, mainfield);
	new mirror(position(0.05, 0.05), position(0.05, 0.25), true, mainfield);
	new mirror(position(0.05, 0.25), position(0.25, 0.25), true, mainfield);
	new mirror(position(0,0), position(0.80,0), false, mainfield);

	QPolygonF poly;
	poly << QPointF(0.25, 0.05) << QPointF(0.25, 0.25)
		 << QPointF(0.45, 0.25);

	new prism(poly, 1.3, mainfield);

	mainfield->recalc();
	mainfield->corner_turn(-0.1, -0.1);
}
