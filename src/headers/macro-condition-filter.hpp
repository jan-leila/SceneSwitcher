#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

enum class FilterCondition {
	ENABLED,
	DISABLED,
	SETTINGS,
};

class MacroConditionFilter : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionFilter>();
	}

	OBSWeakSource _source;
	OBSWeakSource _filter;
	FilterCondition _condition = FilterCondition::ENABLED;
	std::string _settings = "";
	bool _regex = false;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionFilterEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionFilterEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionFilter> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionFilterEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionFilter>(cond));
	}

private slots:
	void SourceChanged(const QString &text);
	void FilterChanged(const QString &text);
	void ConditionChanged(int cond);
	void GetSettingsClicked();
	void SettingsChanged();
	void RegexChanged(int);

protected:
	QComboBox *_sources;
	QComboBox *_filters;
	QComboBox *_conditions;
	QPushButton *_getSettings;
	QPlainTextEdit *_settings;
	QCheckBox *_regex;

	std::shared_ptr<MacroConditionFilter> _entryData;

private:
	void SetSettingsSelectionVisible(bool visible);
	bool _loading = true;
};
