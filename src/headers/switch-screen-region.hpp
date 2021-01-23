#pragma once
#include <QSpinBox>
#include "switch-generic.hpp"

constexpr auto screen_region_func = 4;
constexpr auto default_priority_4 = screen_region_func;

struct ScreenRegionSwitch : SceneSwitcherEntry {
	static bool pause;
	int minX = 0, minY = 0, maxX = 0, maxY = 0;

	const char *getType() { return "region"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);
};

class ScreenRegionWidget : public SwitchWidget {
	Q_OBJECT

public:
	ScreenRegionWidget(QWidget *parent, ScreenRegionSwitch *s);
	ScreenRegionSwitch *getSwitchData();
	void setSwitchData(ScreenRegionSwitch *s);

	static void swapSwitchData(ScreenRegionWidget *s1,
				   ScreenRegionWidget *s2);

	void showFrame();
	void hideFrame();

private slots:
	void MinXChanged(int pos);
	void MinYChanged(int pos);
	void MaxXChanged(int pos);
	void MaxYChanged(int pos);

private:
	QSpinBox *minX;
	QSpinBox *minY;
	QSpinBox *maxX;
	QSpinBox *maxY;

	QFrame helperFrame;

	ScreenRegionSwitch *switchData;

	void drawFrame();
};
