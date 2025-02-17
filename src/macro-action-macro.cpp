#include "headers/macro-action-macro.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionMacro::id = "macro";

bool MacroActionMacro::_registered = MacroActionFactory::Register(
	MacroActionMacro::id,
	{MacroActionMacro::Create, MacroActionMacroEdit::Create,
	 "AdvSceneSwitcher.action.macro"});

const static std::map<PerformMacroAction, std::string> actionTypes = {
	{PerformMacroAction::PAUSE, "AdvSceneSwitcher.action.macro.type.pause"},
	{PerformMacroAction::UNPAUSE,
	 "AdvSceneSwitcher.action.macro.type.unpause"},
	{PerformMacroAction::RESET_COUNTER,
	 "AdvSceneSwitcher.action.macro.type.resetCounter"},
};

bool MacroActionMacro::PerformAction()
{
	if (!_macro.get()) {
		return true;
	}

	switch (_action) {
	case PerformMacroAction::PAUSE:
		_macro->SetPaused();
		break;
	case PerformMacroAction::UNPAUSE:
		_macro->SetPaused(false);
		break;
	case PerformMacroAction::RESET_COUNTER:
		_macro->ResetCount();
		break;
	default:
		break;
	}
	return true;
}

void MacroActionMacro::LogAction()
{
	if (!_macro.get()) {
		return;
	}
	switch (_action) {
	case PerformMacroAction::PAUSE:
		vblog(LOG_INFO, "paused \"%s\"", _macro->Name().c_str());
		break;
	case PerformMacroAction::UNPAUSE:
		vblog(LOG_INFO, "unpaused \"%s\"", _macro->Name().c_str());
		break;
	case PerformMacroAction::RESET_COUNTER:
		vblog(LOG_INFO, "reset counter for \"%s\"",
		      _macro->Name().c_str());
		break;
	default:
		break;
	}
}

bool MacroActionMacro::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	_macro.Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionMacro::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_macro.Load(obj);
	_action = static_cast<PerformMacroAction>(
		obs_data_get_int(obj, "action"));
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionMacroEdit::MacroActionMacroEdit(
	QWidget *parent, std::shared_ptr<MacroActionMacro> entryData)
	: QWidget(parent)
{
	_macros = new MacroSelection(parent);
	_actions = new QComboBox();

	populateActionSelection(_actions);

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(parent, SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{actions}}", _actions},
		{"{{macros}}", _macros},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.macro.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionMacroEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_macros->SetCurrentMacro(_entryData->_macro.get());
}

void MacroActionMacroEdit::MacroChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_macro.UpdateRef(text);
}

void MacroActionMacroEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<PerformMacroAction>(value);
}

void MacroActionMacroEdit::MacroRemove(const QString &name)
{
	UNUSED_PARAMETER(name);
	if (_entryData) {
		_entryData->_macro.UpdateRef();
	}
}
