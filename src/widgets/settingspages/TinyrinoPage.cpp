#include "widgets/settingspages/TinyrinoPage.hpp"

#include "Application.hpp"
#include "controllers/tinyemotesinstances/TinyemotesInstanceModel.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/LayoutCreator.hpp"
#include "widgets/helper/EditableModelView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QHeaderView>
#include <QTableView>

#include <utility>

namespace chatterino {

TinyrinoPage::TinyrinoPage()
{
    LayoutCreator<TinyrinoPage> layoutCreator(this);

    auto layout = layoutCreator.setLayoutType<QVBoxLayout>();
    auto tabs = layout.emplace<QTabWidget>();
    this->tabWidget_ = tabs.getElement();

    auto *s = getSettings();

    auto generalTab = tabs.appendTab(new QVBoxLayout, "General");
    {
        QCheckBox *enableEncryptionCheckbox = this->createCheckBox(
            "Enable message encryption", getSettings()->enableMessageEncryption,
            "Enable message encryption");
        generalTab.append(enableEncryptionCheckbox);

        generalTab.emplace<QLabel>("Password:");

        auto *passwordInput = new QLineEdit();
        passwordInput->setMaximumWidth(280);
        generalTab.append(passwordInput);

        auto &passwordSetting = getSettings()->messagePassword;
        passwordInput->setText(passwordSetting);

        QObject::connect(passwordInput, &QLineEdit::textChanged,
                         [&passwordSetting](const QString &s) {
                             passwordSetting = s;
                         });

        generalTab.emplace<QLabel>("Encryption encoding:");
        auto *combo = generalTab.emplace<QComboBox>().getElement();
        combo->addItems({"Don't use", "Hebrew"});

        auto &setting = getSettings()->messageEncryptionEncoding;
        setting.connect([combo](const int value) {
            combo->setCurrentIndex(value);
        });

        QObject::connect(combo,
                         QOverload<int>::of(&QComboBox::currentIndexChanged),
                         [&setting](int index) {
                             if (index != -1)
                             {
                                 setting = index;
                             }
                         });

        QCheckBox *preferRoleBadges =
            this->createCheckBox("Prefer role badges over custom badges",
                                 getSettings()->preferTinyRoleBadgesOverCustom,
                                 "Changes the order of checking user badges");
        generalTab.append(preferRoleBadges);
    }

    auto instancesTab = tabs.appendTab(new QVBoxLayout, "TinyEmotes instances");
    {
        instancesTab.emplace<QLabel>(
            ""
            "Tinyrino allows you to use emotes "
            "from different TinyEmotes instances.\nYou can add an existing "
            "instance here or self-host your own!");
        auto linkLabel = instancesTab.emplace<QLabel>(
            "<a href='https://github.com/ilotterytea/tinyemotes' "
            "style='color:#99f'>More info...</a>");
        linkLabel->setOpenExternalLinks(true);

        EditableModelView *view =
            instancesTab
                .emplace<EditableModelView>(
                    (new TinyemotesInstanceModel(nullptr))
                        ->initialized(&getSettings()->tinyemotesInstances))
                .getElement();
        this->view_ = view;

        view->setTitles({"Base URL", "Global emotes", "Channel emotes",
                         "Avatars", "Badges"});
        view->getTableView()->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Interactive);
        view->getTableView()->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::Stretch);

        QTimer::singleShot(1, [view] {
            view->getTableView()->resizeColumnsToContents();
            view->getTableView()->setColumnWidth(1, 125);
            view->getTableView()->setColumnWidth(2, 125);
            view->getTableView()->setColumnWidth(3, 110);
            view->getTableView()->setColumnWidth(4, 110);
        });

        // We can safely ignore this signal connection since we own the view
        std::ignore = view->addButtonPressed.connect([] {
            getSettings()->tinyemotesInstances.append(
                TinyemotesInstance("alright.party", true, true, true, true));
        });
    }
}

void TinyrinoPage::onShow()
{
}

QCheckBox *TinyrinoPage::createCheckBox(
    const QString &text, pajlada::Settings::Setting<bool> &setting,
    const QString &toolTipText)
{
    QCheckBox *checkbox = new SCheckBox(text);
    checkbox->setToolTip(toolTipText);

    // update when setting changes
    setting.connect(
        [checkbox](const bool &value, auto) {
            checkbox->setChecked(value);
        },
        this->managedConnections_);

    // update setting on toggle
    QObject::connect(checkbox, &QCheckBox::toggled, this,
                     [&setting](bool state) {
                         setting = state;
                         getApp()->getWindows()->forceLayoutChannelViews();
                     });

    return checkbox;
}

}  // namespace chatterino
