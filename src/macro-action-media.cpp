#include "headers/macro-action-media.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionMedia::id = "media";

bool MacroActionMedia::_registered = MacroActionFactory::Register(
	MacroActionMedia::id,
	{MacroActionMedia::Create, MacroActionMediaEdit::Create,
	 "AdvSceneSwitcher.action.media"});

const static std::map<MediaAction, std::string> actionTypes = {
	{MediaAction::PLAY, "AdvSceneSwitcher.action.media.type.play"},
	{MediaAction::PAUSE, "AdvSceneSwitcher.action.media.type.pause"},
	{MediaAction::STOP, "AdvSceneSwitcher.action.media.type.stop"},
	{MediaAction::RESTART, "AdvSceneSwitcher.action.media.type.restart"},
	{MediaAction::NEXT, "AdvSceneSwitcher.action.media.type.next"},
	{MediaAction::PREVIOUS, "AdvSceneSwitcher.action.media.type.previous"},
};

bool MacroActionMedia::PerformAction()
{
	auto source = obs_weak_source_get_source(_mediaSource);
	obs_media_state state = obs_source_media_get_state(source);
	switch (_action) {
	case MediaAction::PLAY:
		if (state == OBS_MEDIA_STATE_STOPPED ||
		    state == OBS_MEDIA_STATE_ENDED) {
			obs_source_media_restart(source);
		} else {
			obs_source_media_play_pause(source, false);
		}
		break;
	case MediaAction::PAUSE:
		obs_source_media_play_pause(source, true);
		break;
	case MediaAction::STOP:
		obs_source_media_stop(source);
		break;
	case MediaAction::RESTART:
		obs_source_media_restart(source);
		break;
	case MediaAction::NEXT:
		obs_source_media_next(source);
		break;
	case MediaAction::PREVIOUS:
		obs_source_media_previous(source);
		break;
	default:
		break;
	}
	obs_source_release(source);
	return true;
}

void MacroActionMedia::LogAction()
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO, "performed action \"%s\" for source \"%s\"",
		      it->second.c_str(),
		      GetWeakSourceName(_mediaSource).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown media action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionMedia::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "mediaSource",
			    GetWeakSourceName(_mediaSource).c_str());
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionMedia::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	const char *MediaSourceName = obs_data_get_string(obj, "mediaSource");
	_mediaSource = GetWeakSourceByName(MediaSourceName);
	_action = static_cast<MediaAction>(obs_data_get_int(obj, "action"));
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionMediaEdit::MacroActionMediaEdit(
	QWidget *parent, std::shared_ptr<MacroActionMedia> entryData)
	: QWidget(parent)
{
	_mediaSources = new QComboBox();
	_actions = new QComboBox();

	populateActionSelection(_actions);
	populateMediaSelection(_mediaSources);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_mediaSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{mediaSources}}", _mediaSources},
		{"{{actions}}", _actions},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.media.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionMediaEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_mediaSources->setCurrentText(
		GetWeakSourceName(_entryData->_mediaSource).c_str());
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
}

void MacroActionMediaEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_mediaSource = GetWeakSourceByQString(text);
}

void MacroActionMediaEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<MediaAction>(value);
}
