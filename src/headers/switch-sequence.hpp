#pragma once
#include <QVBoxLayout>

#include "switch-generic.hpp"
#include "duration-control.hpp"

constexpr auto round_trip_func = 1;
constexpr auto default_priority_1 = round_trip_func;

struct SceneSequenceSwitch : SceneSwitcherEntry {
	static bool pause;
	SwitchTargetType startTargetType = SwitchTargetType::Scene;
	OBSWeakSource startScene = nullptr;
	Duration delay;
	bool interruptible = false;

	// nullptr marks start point and reaching end of extended sequence
	SceneSequenceSwitch *activeSequence = nullptr;

	std::unique_ptr<SceneSequenceSwitch> extendedSequence = nullptr;

	const char *getType() { return "sequence"; }
	bool initialized();
	bool valid();
	void save(obs_data_t *obj, bool saveExt = true);
	void load(obs_data_t *obj, bool saveExt = true);

	bool reduce();
	SceneSequenceSwitch *extend();

	bool checkMatch(OBSWeakSource currentScene, int &linger,
			SceneSequenceSwitch *root = nullptr);
	bool checkDurationMatchInterruptible();
	void prepareUninterruptibleMatch(int &linger);
	void advanceActiveSequence();
	void logAdvanceSequence();
	void logSequenceCanceled();
};

class SequenceWidget : public SwitchWidget {
	Q_OBJECT

public:
	SequenceWidget(QWidget *parent, SceneSequenceSwitch *s,
		       bool extendSequence = false, bool editExtendMode = false,
		       bool showExtendText = true);
	SceneSequenceSwitch *getSwitchData();
	void setSwitchData(SceneSequenceSwitch *s);

	static void swapSwitchData(SequenceWidget *s1, SequenceWidget *s2);

	void UpdateWidgetStatus(bool showExtendText);
	void setExtendedSequenceStartScene();

private slots:
	void SceneChanged(const QString &text);
	void DelayChanged(double delay);
	void DelayUnitsChanged(DurationUnit);
	void StartSceneChanged(const QString &text);
	void InterruptibleChanged(int state);
	void ExtendClicked();
	void ReduceClicked();

protected:
	DurationSelection *delay;
	QComboBox *startScenes;
	QCheckBox *interruptible;
	QVBoxLayout *extendSequenceLayout;
	QPushButton *extend;
	QPushButton *reduce;

	// I would prefer having a list of only widgets of type editExtendMode
	// but I am not sure how to best implement that using a QListWidget.
	//
	// So use edit button to bring up edit widget and
	// add a label to disaplay current extended sequence state.
	QLabel *extendText;

	SceneSequenceSwitch *switchData;
};
