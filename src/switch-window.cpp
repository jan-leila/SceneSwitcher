#include <regex>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

bool WindowSwitch::pause = false;
static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_windowAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->windowSwitches.emplace_back();

	listAddClicked(ui->windowSwitches,
		       new WindowSwitchWidget(this,
					      &switcher->windowSwitches.back()),
		       ui->windowAdd, &addPulse);

	ui->windowHelp->setVisible(false);
}

void AdvSceneSwitcher::on_windowRemove_clicked()
{
	QListWidgetItem *item = ui->windowSwitches->currentItem();
	if (!item) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->windowSwitches->currentRow();
		auto &switches = switcher->windowSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_windowUp_clicked()
{
	int index = ui->windowSwitches->currentRow();
	if (!listMoveUp(ui->windowSwitches)) {
		return;
	}

	WindowSwitchWidget *s1 =
		(WindowSwitchWidget *)ui->windowSwitches->itemWidget(
			ui->windowSwitches->item(index));
	WindowSwitchWidget *s2 =
		(WindowSwitchWidget *)ui->windowSwitches->itemWidget(
			ui->windowSwitches->item(index - 1));
	WindowSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->windowSwitches[index],
		  switcher->windowSwitches[index - 1]);
}

void AdvSceneSwitcher::on_windowDown_clicked()
{
	int index = ui->windowSwitches->currentRow();

	if (!listMoveDown(ui->windowSwitches)) {
		return;
	}

	WindowSwitchWidget *s1 =
		(WindowSwitchWidget *)ui->windowSwitches->itemWidget(
			ui->windowSwitches->item(index));
	WindowSwitchWidget *s2 =
		(WindowSwitchWidget *)ui->windowSwitches->itemWidget(
			ui->windowSwitches->item(index + 1));
	WindowSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->windowSwitches[index],
		  switcher->windowSwitches[index + 1]);
}

void AdvSceneSwitcher::on_ignoreWindowsAdd_clicked()
{
	QString windowName = ui->ignoreWindowsWindows->currentText();

	if (windowName.isEmpty()) {
		return;
	}

	QVariant v = QVariant::fromValue(windowName);

	QList<QListWidgetItem *> items =
		ui->ignoreWindows->findItems(windowName, Qt::MatchExactly);

	if (items.size() == 0) {
		QListWidgetItem *item =
			new QListWidgetItem(windowName, ui->ignoreWindows);
		item->setData(Qt::UserRole, v);

		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->ignoreWindowsSwitches.emplace_back(
			windowName.toUtf8().constData());
	}

	ui->ignoreWindowHelp->setVisible(false);
}

void AdvSceneSwitcher::on_ignoreWindowsRemove_clicked()
{
	QListWidgetItem *item = ui->ignoreWindows->currentItem();
	if (!item) {
		return;
	}

	QString windowName = item->data(Qt::UserRole).toString();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &switches = switcher->ignoreWindowsSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s == windowName.toUtf8().constData()) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

int AdvSceneSwitcher::IgnoreWindowsFindByData(const QString &window)
{
	int count = ui->ignoreWindows->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->ignoreWindows->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == window) {
			idx = i;
			break;
		}
	}

	return idx;
}

void AdvSceneSwitcher::on_ignoreWindows_currentRowChanged(int idx)
{
	if (loading) {
		return;
	}
	if (idx == -1) {
		return;
	}

	QListWidgetItem *item = ui->ignoreWindows->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->ignoreWindowsSwitches) {
		if (window.compare(s.c_str()) == 0) {
			ui->ignoreWindowsWindows->setCurrentText(s.c_str());
			break;
		}
	}
}

void checkWindowTitleSwitchDirect(WindowSwitch &s,
				  std::string &currentWindowTitle, bool &match)
{
	bool focus = (!s.focus || s.window == currentWindowTitle);
	bool fullscreen = (!s.fullscreen || isFullscreen(s.window));
	bool max = (!s.maximized || isMaximized(s.window));

	if (focus && fullscreen && max) {
		match = true;
	}
}

void checkWindowTitleSwitchRegex(WindowSwitch &s,
				 std::string &currentWindowTitle,
				 std::vector<std::string> windowList,
				 bool &match)
{
	for (auto &window : windowList) {
		try {
			std::regex expr(s.window);
			if (!std::regex_match(window, expr)) {
				continue;
			}
		} catch (const std::regex_error &) {
		}

		bool focus = (!s.focus || window == currentWindowTitle);
		bool fullscreen = (!s.fullscreen || isFullscreen(window));
		bool max = (!s.maximized || isMaximized(window));

		if (focus && fullscreen && max) {
			match = true;
		}
	}
}

bool SwitcherData::checkWindowTitleSwitch(OBSWeakSource &scene,
					  OBSWeakSource &transition)
{
	if (WindowSwitch::pause) {
		return false;
	}

	std::string currentWindowTitle;
	GetCurrentWindowTitle(currentWindowTitle);
	bool match = false;

	// Check if current window is ignored
	for (auto &window : ignoreWindowsSwitches) {
		bool equals = (currentWindowTitle == window);

		try {
			std::regex expr(window);
			bool matches =
				std::regex_match(currentWindowTitle, expr);

			if (equals || matches) {
				currentWindowTitle = lastTitle;
				break;
			}
		} catch (const std::regex_error &) {
		}
	}

	std::vector<std::string> windowList;
	GetWindowList(windowList);

	for (WindowSwitch &s : windowSwitches) {
		if (!s.initialized()) {
			continue;
		}
		if (!s.keepMatching) {
			if(lastMatch == &s || currentWindowTitle == lastTitle){
				continue;
			}
		}

		if (std::find(windowList.begin(), windowList.end(), s.window) !=
		    windowList.end()) {
			checkWindowTitleSwitchDirect(s, currentWindowTitle, match);
			if(match && !s.keepMatching){
				checkWindowTitleSwitchDirect(s, lastTitle, match);
			}
		} else {
			checkWindowTitleSwitchRegex(s, currentWindowTitle, windowList, match);
			if(match && !s.keepMatching){
				checkWindowTitleSwitchRegex(s, lastTitle, windowList, match);
			}
		}

		if (match) {
			lastMatch = &s;

			scene = s.getScene();
			transition = s.transition;
			if (verbose) {
				s.logMatch();
			}
			break;
		}
	}

	lastTitle = currentWindowTitle;

	return match;
}

void SwitcherData::saveWindowTitleSwitches(obs_data_t *obj)
{
	obs_data_array_t *windowTitleArray = obs_data_array_create();
	for (WindowSwitch &s : windowSwitches) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(windowTitleArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "switches", windowTitleArray);
	obs_data_array_release(windowTitleArray);

	obs_data_array_t *ignoreWindowsArray = obs_data_array_create();
	for (std::string &window : ignoreWindowsSwitches) {
		obs_data_t *array_obj = obs_data_create();
		obs_data_set_string(array_obj, "ignoreWindow", window.c_str());
		obs_data_array_push_back(ignoreWindowsArray, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "ignoreWindows", ignoreWindowsArray);
	obs_data_array_release(ignoreWindowsArray);
}

void SwitcherData::loadWindowTitleSwitches(obs_data_t *obj)
{
	windowSwitches.clear();

	obs_data_array_t *windowTitleArray =
		obs_data_get_array(obj, "switches");
	size_t count = obs_data_array_count(windowTitleArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(windowTitleArray, i);

		windowSwitches.emplace_back();
		windowSwitches.back().load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(windowTitleArray);

	ignoreWindowsSwitches.clear();

	obs_data_array_t *ignoreWindowsArray =
		obs_data_get_array(obj, "ignoreWindows");
	count = obs_data_array_count(ignoreWindowsArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(ignoreWindowsArray, i);

		const char *window =
			obs_data_get_string(array_obj, "ignoreWindow");

		ignoreWindowsSwitches.emplace_back(window);

		obs_data_release(array_obj);
	}
	obs_data_array_release(ignoreWindowsArray);
}

void AdvSceneSwitcher::setupTitleTab()
{
	for (auto &s : switcher->windowSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->windowSwitches);
		ui->windowSwitches->addItem(item);
		WindowSwitchWidget *sw = new WindowSwitchWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->windowSwitches->setItemWidget(item, sw);
	}

	if (switcher->windowSwitches.size() == 0) {
		if (!switcher->disableHints) {
			addPulse =
				PulseWidget(ui->windowAdd, QColor(Qt::green));
		}
		ui->windowHelp->setVisible(true);
	} else {
		ui->windowHelp->setVisible(false);
	}

	populateWindowSelection(ui->ignoreWindowsWindows);

	for (auto &window : switcher->ignoreWindowsSwitches) {
		QString text = QString::fromStdString(window);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->ignoreWindows);
		item->setData(Qt::UserRole, text);
	}

	if (switcher->ignoreWindowsSwitches.size() == 0) {
		ui->ignoreWindowHelp->setVisible(true);
	} else {
		ui->ignoreWindowHelp->setVisible(false);
	}
}

void WindowSwitch::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj);

	obs_data_set_string(obj, "windowTitle", window.c_str());
	obs_data_set_bool(obj, "fullscreen", fullscreen);
	obs_data_set_bool(obj, "maximized", maximized);
	obs_data_set_bool(obj, "focus", focus);
	obs_data_set_bool(obj, "keepMatching", keepMatching);
}

void WindowSwitch::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj);

	window = obs_data_get_string(obj, "windowTitle");
	fullscreen = obs_data_get_bool(obj, "fullscreen");
	maximized = obs_data_get_bool(obj, "maximized");
	focus = obs_data_get_bool(obj, "focus") ||
		!obs_data_has_user_value(obj, "focus");
	keepMatching = obs_data_get_bool(obj, "keepMatching");
}

WindowSwitchWidget::WindowSwitchWidget(QWidget *parent, WindowSwitch *s)
	: SwitchWidget(parent, s, true, true)
{
	windows = new QComboBox();
	fullscreen = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.windowTitleTab.fullscreen"));
	maximized = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.windowTitleTab.maximized"));
	focused = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.windowTitleTab.focused"));
	// MYTODO: text
	keepMatching = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.windowTitleTab.keepMatching"));

	QWidget::connect(windows, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(WindowChanged(const QString &)));
	QWidget::connect(fullscreen, SIGNAL(stateChanged(int)), this,
			 SLOT(FullscreenChanged(int)));
	QWidget::connect(maximized, SIGNAL(stateChanged(int)), this,
			 SLOT(MaximizedChanged(int)));
	QWidget::connect(focused, SIGNAL(stateChanged(int)), this,
			 SLOT(FocusChanged(int)));
	QWidget::connect(keepMatching, SIGNAL(stateChanged(int)), this,
			 SLOT(KeepMatchingChanged(int)));

	populateWindowSelection(windows);

	windows->setEditable(true);
	windows->setMaxVisibleItems(20);

	if (s) {
		windows->setCurrentText(s->window.c_str());
		fullscreen->setChecked(s->fullscreen);
		maximized->setChecked(s->maximized);
		focused->setChecked(s->focus);
		keepMatching->setChecked(s->keepMatching);
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{windows}}", windows},
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions},
		{"{{fullscreen}}", fullscreen},
		{"{{maximized}}", maximized},
		{"{{focused}}", focused},
		{"{{keepMatching}}", keepMatching}};
	placeWidgets(obs_module_text("AdvSceneSwitcher.windowTitleTab.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

WindowSwitch *WindowSwitchWidget::getSwitchData()
{
	return switchData;
}

void WindowSwitchWidget::setSwitchData(WindowSwitch *s)
{
	switchData = s;
}

void WindowSwitchWidget::swapSwitchData(WindowSwitchWidget *s1,
					WindowSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	WindowSwitch *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void WindowSwitchWidget::WindowChanged(const QString &text)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->window = text.toStdString();
}

void WindowSwitchWidget::FullscreenChanged(int state)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->fullscreen = state;
}

void WindowSwitchWidget::MaximizedChanged(int state)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->maximized = state;
}

void WindowSwitchWidget::FocusChanged(int state)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->focus = state;
}


void WindowSwitchWidget::KeepMatchingChanged(int state)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->keepMatching = state;
}
