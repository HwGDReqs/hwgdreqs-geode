#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <cctype>
#include <fstream>

using namespace geode::prelude;

static const std::string REPO = "HwGDReqs/hwgdreqs-geode";
static const std::string UPDATE_URL = "https://github.com/" + REPO + "/releases/latest/download/geekedgdplayer.click-guide-visualizer.geode";
static bool g_updateChecked = false;

int parseFirstInt(std::string const& s) {
    int sign = 1;
    size_t i = 0;
    while (i < s.size() && !std::isdigit(static_cast<unsigned char>(s[i])) && s[i] != '-') {
        i++;
    }
    if (i < s.size() && s[i] == '-') {
        sign = -1;
        i++;
    }
    int value = 0;
    bool found = false;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        found = true;
        value = value * 10 + (s[i] - '0');
        i++;
    }
    return found ? value * sign : 0;
}

std::string getHwgdreqsBaseUrl() {
    auto customLink = Mod::get()->getSettingValue<std::string>("custom-api-link");
    if (!customLink.empty()) {
        return customLink;
    }
    auto customPort = Mod::get()->getSettingValue<std::string>("custom-api-port");
    return fmt::format("http://localhost:{}", customPort);
} // yippee


struct RequestData {
    int id;
    std::string name;
    std::string difficulty;
    std::string author;
    std::string requester;
    std::string platform;
    std::string message;
    std::string length;
    bool large;
    bool two_player;
    bool bad_requester;
    std::string bad_requester_reason;
};

// blacklist popup here (lazy to split to other files for now)
class BlacklistPopup : public geode::Popup {
public:
    RequestData m_data;
    geode::Function<void()> m_onRefresh;

    static BlacklistPopup* create(RequestData const& data, geode::Function<void()> onRefresh) {
        auto ret = new BlacklistPopup();
        ret->m_data = data;
        ret->m_onRefresh = std::move(onRefresh);
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

protected:
    bool init() override {
        if (!Popup::init(350.f, 275.f))
            return false;
        this->setTitle("Blacklist Options");
        auto menu = CCMenu::create();
        menu->setPosition(m_mainLayer->getContentSize() / 2);
        float startY = 80.f;
        float gap = 50.f;

        // blacklist level
        auto blLevelBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Blacklist Level", "goldFont.fnt", "GJ_button_01.png", 0.8f),
            this,
            menu_selector(BlacklistPopup::onBlacklistLevel)
        );
        blLevelBtn->setPosition(0.f, startY);
        menu->addChild(blLevelBtn);

        // blacklist author
        auto blAuthorBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Blacklist Author", "goldFont.fnt", "GJ_button_01.png", 0.8f),
            this,
            menu_selector(BlacklistPopup::onBlacklistAuthor)
        );
        blAuthorBtn->setPosition(0.f, startY - gap);
        menu->addChild(blAuthorBtn);

        // blacklist requester
        auto blRequesterBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Blacklist Requester", "goldFont.fnt", "GJ_button_01.png", 0.8f),
            this,
            menu_selector(BlacklistPopup::onBlacklistRequester)
        );
        blRequesterBtn->setPosition(0.f, startY - gap * 2);
        menu->addChild(blRequesterBtn);

        // ban requester (twitch)
        if (m_data.platform == "twitch") {
            auto banRequesterBtn = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Ban Requester", "goldFont.fnt", "GJ_button_01.png", 0.8f),
                this,
                menu_selector(BlacklistPopup::onBanRequester)
            );
            banRequesterBtn->setPosition(0.f, startY - gap * 3);
            menu->addChild(banRequesterBtn);
        }

        m_mainLayer->addChild(menu);
        return true;
    }

    void onBlacklistLevel(CCObject*) {
        auto req = web::WebRequest();
        auto url = fmt::format("{}/blacklistlevel?id={}", getHwgdreqsBaseUrl(), m_data.id);
        async::spawn(
            req.post(url),
            [this](web::WebResponse res) {
                if (!res.ok()) {
                    FLAlertLayer::create("Error", HWGDREQS_ERR_MSG, "OK")->show();
                    return;
                }
                geode::Notification::create("Blacklisted (level)", NotificationIcon::Success)->show();
            }
        );
    }

    void onBlacklistAuthor(CCObject*) {
        auto req = web::WebRequest();
        auto url = fmt::format("{}{}{}", getHwgdreqsBaseUrl(), "/banauthor?id=", m_data.id);
        async::spawn(
            req.post(url),
            [this](web::WebResponse res) {
                if (!res.ok()) {
                    FLAlertLayer::create("Error", HWGDREQS_ERR_MSG, "OK")->show();
                    return;
                }
                geode::Notification::create("Blacklisted (creator)", NotificationIcon::Success)->show();
            }
        );
    }

    void onBlacklistRequester(CCObject*) {
        auto req = web::WebRequest();
        auto url = fmt::format("{}{}{}", getHwgdreqsBaseUrl(), "/banrequester?id=", m_data.id);
        async::spawn(
            req.post(url),
            [this](web::WebResponse res) {
                if (!res.ok()) {
                    FLAlertLayer::create("Error", HWGDREQS_ERR_MSG, "OK")->show();
                    return;
                }
                geode::Notification::create("Blacklisted (requester)", NotificationIcon::Success)->show();
            }
        );
    }

    void onBanRequester(CCObject*) {
        int levelId = m_data.id;
        auto req = web::WebRequest();
        auto url = fmt::format("{}{}{}", getHwgdreqsBaseUrl(), "/bantwitch?id=", levelId);
        async::spawn(
            req.post(url),
            [levelId](web::WebResponse res) {
                auto jsonRes = res.json();
                if (jsonRes.isOk()) {
                    auto data = jsonRes.unwrap();
                    if (data.contains("error")) {
                        auto errorKey = data["error"].asString().unwrapOr("");
                        if (errorKey == "moderation_not_enabled") {
                            geode::Notification::create("Moderation not on, re-login on desktop app", NotificationIcon::Error)->show();
                        } else if (errorKey == "cannot_ban_self") {
                            geode::Notification::create("SON you can't ban yourself", NotificationIcon::Error)->show();
                        } else {
                            geode::Notification::create("Error: " + errorKey, NotificationIcon::Error)->show();
                        }
                        return;
                    }
                    geode::Notification::create("alr Ban happened", NotificationIcon::Success)->show();
                    return;
                }
                
                if (!res.ok()) {
                    FLAlertLayer::create("Error", HWGDREQS_ERR_MSG, "OK")->show();
                    return;
                }
                
                geode::Notification::create("Error: Invalid response", NotificationIcon::Error)->show();
            }
        );
    }

    void keyBackClicked() override {
        this->removeFromParent();
    }
};

class ReqPopup : public geode::Popup, public LevelManagerDelegate {
public:
    RequestData m_data;
    geode::Function<void()> m_onRefresh;
    TaskHolder<web::WebResponse> m_queueCountListener;
    CCLabelBMFont* m_queueCountLabel = nullptr;

    static ReqPopup* create(RequestData const& data, geode::Function<void()> onRefresh) {
        auto ret = new ReqPopup();
        ret->m_data = data;
        ret->m_onRefresh = std::move(onRefresh);
        if (ret->init(data)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

protected:
    bool init(RequestData const& data) {
        if (!Popup::init(380.f, 250.f))
            return false;

        auto levelName = CCLabelBMFont::create(data.name.c_str(), "bigFont.fnt");
        levelName->setPosition({ m_mainLayer->getContentSize().width / 2, m_mainLayer->getContentSize().height / 2 + 90.f });
        levelName->setScale(0.775f);
        levelName->setAlignment(CCTextAlignment::kCCTextAlignmentLeft);
        m_mainLayer->addChild(levelName);

        auto diffIcon = CCSprite::createWithSpriteFrameName(getDifficultyIcon(data.difficulty).c_str());
        diffIcon->setPosition({ m_mainLayer->getContentSize().width / 2 - 155.f, m_mainLayer->getContentSize().height / 2 + 90.f });
        m_mainLayer->addChild(diffIcon);

        auto playBtnSpr = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png");
        auto playBtn = CCMenuItemSpriteExtra::create(
            playBtnSpr,
            this,
            menu_selector(ReqPopup::onPlay)
        );
        playBtn->setPosition({ m_mainLayer->getContentSize().width / 2 - 113.f, m_mainLayer->getContentSize().height / 2 });

        // hope this works
        auto authorLabel = CCLabelBMFont::create(fmt::format("by: {}", data.author).c_str(), "chatFont.fnt");
        authorLabel->setPosition({ 250.f, m_mainLayer->getContentSize().height / 2 + 37.f });
        authorLabel->setAlignment(CCTextAlignment::kCCTextAlignmentLeft);
        m_mainLayer->addChild(authorLabel);

        auto requesterLabel = CCLabelBMFont::create(fmt::format("from: {}", data.requester).c_str(), "chatFont.fnt");
        requesterLabel->setPosition({ 250.f, m_mainLayer->getContentSize().height / 2 + 20.f });
        requesterLabel->setAlignment(CCTextAlignment::kCCTextAlignmentLeft);
        m_mainLayer->addChild(requesterLabel);

        auto platformIconName = getPlatformIcon(data.platform);
        if (!platformIconName.empty()) {
            auto platformIcon = CCSprite::createWithSpriteFrameName(platformIconName.c_str());
            platformIcon->setPosition({ m_mainLayer->getContentSize().width / 2 + 157.f, m_mainLayer->getContentSize().height / 2 + 96.f });
            m_mainLayer->addChild(platformIcon);
        }

        auto timeIcon = CCSprite::createWithSpriteFrameName("GJ_timeIcon_001.png");
        timeIcon->setPosition({ m_mainLayer->getContentSize().width / 2 - 51.f, m_mainLayer->getContentSize().height / 2 - 70.f });
        timeIcon->setScale(0.925f);
        m_mainLayer->addChild(timeIcon);

        auto lengthLabel = CCLabelBMFont::create(data.length.c_str(), "bigFont.fnt");
        lengthLabel->setPosition({ 200.f, m_mainLayer->getContentSize().height / 2 - 70.f });
        lengthLabel->setScale(0.575f);
        lengthLabel->setAlignment(CCTextAlignment::kCCTextAlignmentLeft);
        m_mainLayer->addChild(lengthLabel);

        if (data.large) {
            auto largeIcon = CCSprite::createWithSpriteFrameName("highObjectIcon_001.png");
            largeIcon->setPosition({ m_mainLayer->getContentSize().width / 2 - 39.f, m_mainLayer->getContentSize().height / 2 - 99.f });
            m_mainLayer->addChild(largeIcon);
        }

        if (data.two_player) {
            auto twoPlayerLabel = CCLabelBMFont::create("2p", "bigFont.fnt");
            twoPlayerLabel->setPosition({ m_mainLayer->getContentSize().width / 2 - 16.f, m_mainLayer->getContentSize().height / 2 - 97.f });
            twoPlayerLabel->setScale(0.575f);
            twoPlayerLabel->setAlignment(CCTextAlignment::kCCTextAlignmentLeft);
            m_mainLayer->addChild(twoPlayerLabel);
        }

        // this sprite's good? i got this in mind and seemed good :3
        auto nextBtnSpr = CircleButtonSprite::create(
            CCSprite::createWithSpriteFrameName("navArrowBtn_001.png"),
            CircleBaseColor::Green,
            CircleBaseSize::SmallAlt
        );
        auto nextBtn = CCMenuItemSpriteExtra::create(
            nextBtnSpr,
            this,
            menu_selector(ReqPopup::onNext)
        );
        nextBtn->setPosition({ m_mainLayer->getContentSize().width / 2 + 151.f, m_mainLayer->getContentSize().height / 2 - 91.f });

        auto blacklistBtnSpr = LeaderboardButtonSprite::create(
            CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png"),
            LeaderboardBaseColor::Blue,
            LeaderboardBaseSize::Normal
        );
        auto blacklistBtn = CCMenuItemSpriteExtra::create(
            blacklistBtnSpr,
            this,
            menu_selector(ReqPopup::onBlacklist)
        );
        blacklistBtn->setPosition({ m_mainLayer->getContentSize().width / 2 - 153.f, m_mainLayer->getContentSize().height / 2 - 91.f });

        auto menu = CCMenu::create();
        menu->setPosition({ 0, 0 });
        menu->addChild(playBtn);
        menu->addChild(nextBtn);
        menu->addChild(blacklistBtn);

        // if message doesn't start with !replace
        if (!data.message.starts_with("!replace")) {
            auto msgIconSpr = CCSprite::createWithSpriteFrameName("GJ_chatBtn_001.png");
            auto msgIconBtn = CCMenuItemSpriteExtra::create(
                msgIconSpr,
                this,
                menu_selector(ReqPopup::onMsgIcon)
            );
            msgIconBtn->setPosition({ 147.f, 150.f });
            msgIconBtn->setSizeMult(1.0f);
            menu->addChild(msgIconBtn);
        }

        // bad requester warning
        if (data.bad_requester) {
            auto warnLabel = CCLabelBMFont::create("WARNING: NON TRUSTED REQUESTER (click for more info)", "chatFont.fnt");
            warnLabel->setColor({ 255, 0, 0 });
            warnLabel->setScale(0.6f);
            auto warnItem = CCMenuItemLabel::create(warnLabel, this, menu_selector(ReqPopup::onBadRequesterInfo));
            warnItem->setPosition({ 190.f, 265.f });
            auto warnMenu = CCMenu::create();
            warnMenu->setPosition({ 0.f, 0.f });
            warnMenu->addChild(warnItem);
            m_mainLayer->addChild(warnMenu);
        }

        // queue count label
        m_queueCountLabel = CCLabelBMFont::create("Queue Count: 0", "chatFont.fnt");
        m_queueCountLabel->setPosition({ 325.f, 76.f });
        m_queueCountLabel->setScale(0.775f);
        m_mainLayer->addChild(m_queueCountLabel);

        m_mainLayer->addChild(menu);

        this->fetchQueueCount();

        return true;
    }

    void onBadRequesterInfo(CCObject*) {
        FLAlertLayer::create("Reason", m_data.bad_requester_reason.c_str(), "OK")->show();
    }

    void fetchQueueCount() {
        auto req = web::WebRequest();
        m_queueCountListener.spawn(
            "Fetching Queue Count",
            req.get(getHwgdreqsBaseUrl() + "/queue/count"),
            [this](web::WebResponse res) {
                if (!m_queueCountLabel) return;
                if (!res.ok()) {
                    m_queueCountLabel->setString("Queue Count: 0");
                    return;
                }
                auto body = res.string().unwrapOr("0");
                auto count = parseFirstInt(body);
                m_queueCountLabel->setString(fmt::format("Queue Count: {}", count).c_str());
            }
        );
    }

    std::string getDifficultyIcon(std::string const& difficulty) {
        if (difficulty == "Easy") return "diffIcon_01_btn_001.png";
        else if (difficulty == "Normal") return "diffIcon_02_btn_001.png";
        else if (difficulty == "Hard") return "diffIcon_03_btn_001.png";
        else if (difficulty == "Harder") return "diffIcon_04_btn_001.png";
        else if (difficulty == "Insane") return "diffIcon_05_btn_001.png";
        else if (difficulty == "Easy Demon") return "diffIcon_06_btn_001.png";
        else if (difficulty == "Medium Demon") return "diffIcon_07_btn_001.png";
        else if (difficulty == "Hard Demon") return "diffIcon_08_btn_001.png";
        else if (difficulty == "Insane Demon") return "diffIcon_09_btn_001.png";
        else if (difficulty == "Extreme Demon") return "diffIcon_10_btn_001.png";
        else if (difficulty == "Auto") return "diffIcon_auto_btn_001.png";
        return "diffIcon_00_btn_001.png";
    }

    std::string getPlatformIcon(ZStringView platform) {
        if (platform == "twitch") return "gj_twitchIcon_001.png";
        else if (platform == "youtube") return "gj_ytIcon_001.png";
        return "";
    }

    void onPlay(CCObject*) {
        auto glm = GameLevelManager::sharedState();
        glm->m_levelManagerDelegate = this;

        GJSearchObject* srch = GJSearchObject::create(
            SearchType::Search,
            fmt::format("{}", m_data.id)
        );
        glm->getOnlineLevels(srch);
    }

    void onNext(CCObject*) {
        this->onDelete(nullptr);
    }

    void onDelete(CCObject*) {
        auto req = web::WebRequest();
        auto url = fmt::format("{}/delete?id={}", getHwgdreqsBaseUrl(), m_data.id);
        async::spawn(
            req.post(url),
            [this](web::WebResponse res) {
                if (!res.ok()) {
                    FLAlertLayer::create("Error", HWGDREQS_ERR_MSG, "OK")->show();
                    return;
                }
                if (m_onRefresh) {
                    m_onRefresh();
                }
                this->removeFromParent();
            }
        );
    }

    void onMsgIcon(CCObject*) {
        FLAlertLayer::create(m_data.requester.c_str(), m_data.message.c_str(), "OK")->show();
    }

    void onBlacklist(CCObject*) {
        BlacklistPopup::create(m_data, std::move(m_onRefresh))->show();
    }

    void loadLevelsFinished(CCArray* lvls, char const*, int) override {
        auto glm = GameLevelManager::sharedState();
        glm->m_levelManagerDelegate = nullptr;

        if (!lvls || lvls->count() == 0) return;

        auto lvl = typeinfo_cast<GJGameLevel*>(lvls->objectAtIndex(0));
        if (!lvl) return;
        
        glm->saveLevel(lvl); // malik and da streamers will be HAPPY cuz of this one >:3
        // edit this didnt work, so looking for a way to save the level :sob:

        auto scene = LevelInfoLayer::scene(lvl, false);
        CCDirector::sharedDirector()->replaceScene(
            CCTransitionFade::create(0.5f, scene)
        );
    }
};

static void checkForUpdate() {
    if (g_updateChecked) return;
    g_updateChecked = true;
    geode::async::spawn(
        []() -> web::WebFuture {
            return web::WebRequest()
                .header("User-Agent", "click-guide-visualizer")
                .get("https://api.github.com/repos/" + REPO + "/releases/latest");
        },
        [](web::WebResponse res) {
            if (!res.ok()) return;
            auto jsonRes = res.json();
            if (!jsonRes) return;
            std::string tagName = (*jsonRes)["tag_name"].asString().unwrapOr("");
            if (tagName.empty()) return;
            std::string latestVersion = tagName;
            if (!latestVersion.empty() && latestVersion[0] == 'v') latestVersion = latestVersion.substr(1);
            
            std::string currentVersion = Mod::get()->getVersion().toNonVString();
            
            auto currentParts = geode::utils::string::split(currentVersion, ".");
            auto latestParts = geode::utils::string::split(latestVersion, ".");
            
            bool hasUpdate = false;
            for (size_t i = 0; i < std::max(currentParts.size(), latestParts.size()); i++) {
                int currentPart = (i < currentParts.size()) ? std::stoi(currentParts[i]) : 0;
                int latestPart = (i < latestParts.size()) ? std::stoi(latestParts[i]) : 0;
                if (latestPart > currentPart) {
                    hasUpdate = true;
                    break;
                } else if (latestPart < currentPart) {
                    break;
                }
            }
            
            if (!hasUpdate) return;
            
            auto assets = (*jsonRes)["assets"].asArray();
            std::string downloadUrl;
            std::string sha256;
            for (auto& asset : assets) {
                auto name = asset["name"].asString().unwrapOr("");
                if (name == "geekedgdplayer.click-guide-visualizer.geode") {
                    downloadUrl = asset["browser_download_url"].asString().unwrapOr("");
                    sha256 = asset["sha256"].asString().unwrapOr("");
                    break;
                }
            }
            
            if (downloadUrl.empty()) return;
            
            geode::createQuickPopup(
                "HwGDReqs Update",
                fmt::format("A new version (<cy>{}</c>) is available!\nYou have <cr>{}</c>. Update now?", tagName, currentVersion),
                "Later", "Update",
                [downloadUrl, sha256, tagName](FLAlertLayer*, bool btn2) {
                    if (!btn2) return;
                    Notification::create("Downloading update...", NotificationIcon::Info)->show();
                    geode::async::spawn(
                        [downloadUrl]() -> web::WebFuture {
                            return web::WebRequest().get(downloadUrl);
                        },
                        [downloadUrl, sha256, tagName](web::WebResponse dlRes) {
                            if (!dlRes.ok()) {
                                Notification::create("Update failed!", NotificationIcon::Error)->show();
                                return;
                            }
                            
                            auto path = Mod::get()->getPackagePath();
                            auto downloadPath = path.parent_path() / (path.stem().string() + "-downloading.geode");
                            
                            auto data = dlRes.data();
                            std::ofstream file(downloadPath.string(), std::ios::binary);
                            file.write(reinterpret_cast<const char*>(data.data()), data.size());
                            file.close();
                            
                            auto hash = geode::crypto::sha256(data);
                            std::string hashHex = geode::crypto::hexEncode(hash);
                            
                            if (hashHex != sha256) {
                                std::filesystem::remove(downloadPath);
                                Notification::create("Update failed: Integrity check failed!", NotificationIcon::Error)->show();
                                return;
                            }
                            
                            std::error_code ec;
                            std::filesystem::rename(downloadPath, path, ec);
                            if (ec) {
                                Notification::create("Update failed: Could not replace file!", NotificationIcon::Error)->show();
                                return;
                            }
                            
                            Notification::create("Updated! Restart to apply!", NotificationIcon::Success)->show();
                        }
                    );
                }
            );
        }
    );
}

class $modify(MyMenuLayer, MenuLayer) {
    struct Fields {
        TaskHolder<web::WebResponse> m_webListener;
    };

    bool init() {
        if (!MenuLayer::init()) return false;

        checkForUpdate();

        auto btnSpr = CircleButtonSprite::createWithSprite(
            "button.png"_spr, 0.65f,
            CircleBaseColor::Green,
            CircleBaseSize::Medium
        );
        auto btn = CCMenuItemSpriteExtra::create(
            btnSpr,
            this,
            menu_selector(MyMenuLayer::onMyButton)
        );

        auto menu = this->getChildByID("right-side-menu");
        menu->addChild(btn);
        btn->setID("hwgdreqsBtn"_spr);
        menu->updateLayout();

        return true;
    }

    void fetchAndShowPopup() {
        auto req = web::WebRequest();
        m_fields->m_webListener.spawn(
            "Fetching Request",
            req.get(getHwgdreqsBaseUrl() + "/current"),
            [this](web::WebResponse res) {
                if (!res.ok()) {
                    FLAlertLayer::create("HwGDReqs fail", "failed connecting to app, make sure the desktop app is open", "OK")->show();
                    return;
                }

                auto jsonRes = res.json();
                if (!jsonRes.isOk()) {
                    FLAlertLayer::create("HwGDReqs fail", "Invalid JSON response", "OK")->show();
                    return;
                }

                auto data = jsonRes.unwrap();

                if (!data.contains("level")) { // idk about this exact api
                    FLAlertLayer::create("HwGDReqs", "No level in queue", "OK")->show();
                    return;
                }
                auto levelData = data["level"];
                if (levelData.isNull()) { // idk about this exact api
                    FLAlertLayer::create("HwGDReqs", "No level in queue", "OK")->show();
                    return;
                }

                bool badRequester = levelData.contains("bad_requester") ? levelData["bad_requester"].asBool().unwrapOr(false) : false;
                std::string badRequesterReason = levelData.contains("bad_requester_reason") ? levelData["bad_requester_reason"].asString().unwrapOr("") : "";

                RequestData reqData;
                auto idStr = levelData.contains("id") ? levelData["id"].asString().unwrapOr("0") : "0";
                reqData.id = ::numFromString<int>(idStr).unwrapOr(0);
                reqData.name = levelData.contains("name") ? levelData["name"].asString().unwrapOr("Unknown") : "Unknown";
                reqData.difficulty = levelData.contains("difficulty") ? levelData["difficulty"].asString().unwrapOr("Unrated") : "Unrated";
                reqData.author = levelData.contains("author") ? levelData["author"].asString().unwrapOr("Unknown") : "Unknown";
                reqData.requester = levelData.contains("requester") ? levelData["requester"].asString().unwrapOr("Unknown") : "Unknown";
                reqData.platform = levelData.contains("platform") ? levelData["platform"].asString().unwrapOr("") : "";
                reqData.message = levelData.contains("message") ? levelData["message"].asString().unwrapOr("") : "";
                reqData.length = levelData.contains("length") ? levelData["length"].asString().unwrapOr("Tiny") : "Tiny";
                reqData.large = levelData.contains("large") ? levelData["large"].asBool().unwrapOr(false) : false;
                reqData.two_player = levelData.contains("two_player") ? levelData["two_player"].asBool().unwrapOr(false) : false;
                reqData.bad_requester = badRequester;
                reqData.bad_requester_reason = badRequesterReason;

                ReqPopup::create(reqData, [this] {
                    this->fetchAndShowPopup();
                })->show();
            }
        );
    }

    void onMyButton(CCObject*) {
        fetchAndShowPopup(); // :)
    }
};
