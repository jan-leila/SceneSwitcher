#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-scene.hpp"

std::map<std::string, MacroConditionInfo> MacroConditionFactory::_methods;

bool MacroConditionFactory::Register(const std::string &id,
				     MacroConditionInfo info)
{
	if (auto it = _methods.find(id); it == _methods.end()) {
		_methods[id] = info;
		return true;
	}
	return false;
}

std::shared_ptr<MacroCondition>
MacroConditionFactory::Create(const std::string &id)
{
	if (auto it = _methods.find(id); it != _methods.end()) {
		return it->second._createFunc();
	}
	return nullptr;
}

QWidget *
MacroConditionFactory::CreateWidget(const std::string &id, QWidget *parent,
				    std::shared_ptr<MacroCondition> cond)
{
	if (auto it = _methods.find(id); it != _methods.end()) {
		return it->second._createWidgetFunc(parent, cond);
	}
	return nullptr;
}

std::string MacroConditionFactory::GetConditionName(const std::string &id)
{
	if (auto it = _methods.find(id); it != _methods.end()) {
		return it->second._name;
	}
	return "unknown condition";
}

std::string MacroConditionFactory::GetIdByName(const QString &name)
{
	for (auto it : _methods) {
		if (name == obs_module_text(it.second._name.c_str())) {
			return it.first;
		}
	}
	return "";
}

bool MacroConditionFactory::UsesDurationConstraint(const std::string &id)
{
	if (auto it = _methods.find(id); it != _methods.end()) {
		return it->second._useDurationConstraint;
	}
	return false;
}

static inline void populateLogicSelection(QComboBox *list, bool root = false)
{
	if (root) {
		for (auto entry : MacroCondition::logicTypes) {
			if (static_cast<int>(entry.first) < logic_root_offset) {
				list->addItem(obs_module_text(
					entry.second._name.c_str()));
			}
		}
	} else {
		for (auto entry : MacroCondition::logicTypes) {
			if (static_cast<int>(entry.first) >=
			    logic_root_offset) {
				list->addItem(obs_module_text(
					entry.second._name.c_str()));
			}
		}
	}
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : MacroConditionFactory::GetConditionTypes()) {
		list->addItem(obs_module_text(entry.second._name.c_str()));
	}
}

MacroConditionEdit::MacroConditionEdit(
	QWidget *parent, std::shared_ptr<MacroCondition> *entryData,
	const std::string &id, bool root, bool startCollapsed)
	: QWidget(parent)
{
	_logicSelection = new QComboBox();
	_conditionSelection = new QComboBox();
	_section = new Section(300);
	_dur = new DurationConstraintEdit();

	QWidget::connect(_logicSelection, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(LogicSelectionChanged(int)));
	QWidget::connect(_conditionSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ConditionSelectionChanged(const QString &)));
	QWidget::connect(_dur, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_dur, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(DurationUnitChanged(DurationUnit)));
	QWidget::connect(_dur, SIGNAL(ConditionChanged(DurationCondition)),
			 this,
			 SLOT(DurationConditionChanged(DurationCondition)));

	populateLogicSelection(_logicSelection, root);
	populateConditionSelection(_conditionSelection);

	_section->AddHeaderWidget(_logicSelection);
	_section->AddHeaderWidget(_conditionSelection);
	_section->AddHeaderWidget(_dur);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(_section);
	setLayout(mainLayout);

	_entryData = entryData;
	_isRoot = root;
	UpdateEntryData(id, startCollapsed);
	_loading = false;
}

void MacroConditionEdit::LogicSelectionChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	LogicType type;
	if (IsRootNode()) {
		type = static_cast<LogicType>(idx);
	} else {
		type = static_cast<LogicType>(idx + logic_root_offset);
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	(*_entryData)->SetLogicType(type);
}

bool MacroConditionEdit::IsRootNode()
{
	return _isRoot;
}

void MacroConditionEdit::UpdateEntryData(const std::string &id, bool collapse)
{
	_conditionSelection->setCurrentText(obs_module_text(
		MacroConditionFactory::GetConditionName(id).c_str()));
	auto widget =
		MacroConditionFactory::CreateWidget(id, window(), *_entryData);
	auto logic = (*_entryData)->GetLogicType();
	if (IsRootNode()) {
		_logicSelection->setCurrentIndex(static_cast<int>(logic));
	} else {
		_logicSelection->setCurrentIndex(static_cast<int>(logic) -
						 logic_root_offset);
	}
	_section->SetContent(widget, collapse);

	_dur->setVisible(MacroConditionFactory::UsesDurationConstraint(id));
	auto constraint = (*_entryData)->GetDurationConstraint();
	_dur->SetValue(constraint);
}

void MacroConditionEdit::ConditionSelectionChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::string id = MacroConditionFactory::GetIdByName(text);

	auto temp = DurationConstraint();
	_dur->SetValue(temp);

	std::lock_guard<std::mutex> lock(switcher->m);
	auto logic = (*_entryData)->GetLogicType();
	_entryData->reset();
	*_entryData = MacroConditionFactory::Create(id);
	(*_entryData)->SetLogicType(logic);
	auto widget =
		MacroConditionFactory::CreateWidget(id, window(), *_entryData);
	_section->SetContent(widget, false);
	_dur->setVisible(MacroConditionFactory::UsesDurationConstraint(id));
}

void MacroConditionEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	(*_entryData)->SetDuration(seconds);
}

void MacroConditionEdit::DurationConditionChanged(DurationCondition cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	(*_entryData)->SetDurationCondition(cond);
}

void MacroConditionEdit::DurationUnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	(*_entryData)->SetDurationUnit(unit);
}

void AdvSceneSwitcher::on_conditionAdd_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}
	MacroConditionScene temp;
	std::string id = temp.GetId();

	std::lock_guard<std::mutex> lock(switcher->m);
	bool root = macro->Conditions().size() == 0;
	macro->Conditions().emplace_back(MacroConditionFactory::Create(id));
	auto logic = root ? LogicType::ROOT_NONE : LogicType::NONE;
	macro->Conditions().back()->SetLogicType(logic);
	auto newEntry = new MacroConditionEdit(
		this, &macro->Conditions().back(), id, root, false);
	ui->macroEditConditionLayout->addWidget(newEntry);
	ui->macroEditConditionHelp->setVisible(false);
}

void AdvSceneSwitcher::on_conditionRemove_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}
	std::lock_guard<std::mutex> lock(switcher->m);
	if (macro->Conditions().empty()) {
		return;
	}
	macro->Conditions().pop_back();

	int count = ui->macroEditConditionLayout->count();
	auto item = ui->macroEditConditionLayout->takeAt(count - 1);

	if (item) {
		auto widget = item->widget();
		if (widget) {
			widget->setVisible(false);
		}
		delete item;
	}

	if (count == 1) {
		ui->macroEditConditionHelp->setVisible(true);
	}
}
