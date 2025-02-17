#pragma once
#include "macro-action-edit.hpp"

#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>

enum class SourceAction {
	ENABLE,
	DISABLE,
	SETTINGS,
};

class MacroActionSource : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionSource>();
	}

	OBSWeakSource _source;
	std::string _settings = "";
	SourceAction _action = SourceAction::ENABLE;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionSourceEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSourceEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSource> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSourceEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionSource>(action));
	}

private slots:
	void SourceChanged(const QString &text);
	void ActionChanged(int value);
	void GetSettingsClicked();
	void SettingsChanged();

protected:
	QComboBox *_sources;
	QComboBox *_actions;
	QPushButton *_getSettings;
	QPlainTextEdit *_settings;
	QLabel *_warning;
	std::shared_ptr<MacroActionSource> _entryData;

private:
	void SetWidgetVisibility(bool);
	bool _loading = true;
};
