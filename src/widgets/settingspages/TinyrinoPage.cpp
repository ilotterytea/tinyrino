#include "widgets/settingspages/TinyrinoPage.hpp"

#include "Application.hpp"
#include "controllers/tinyemotesinstances/TinyemotesInstanceModel.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/LayoutCreator.hpp"
#include "widgets/helper/EditableModelView.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QHeaderView>
#include <QTableView>

#include <utility>

namespace chatterino {

TinyrinoPage::TinyrinoPage()
    : view(GeneralPageView::withoutNavigation(this))
{
    auto *y = new QVBoxLayout;
    auto *x = new QHBoxLayout;
    x->addWidget(this->view);
    auto *z = new QFrame;
    z->setLayout(x);
    y->addWidget(z);
    this->setLayout(y);

    this->initLayout(*this->view);
}

void TinyrinoPage::initLayout(GeneralPageView &layout)
{
    auto *s = getSettings();

    {
        layout.addTitle("TinyEmotes");

        SettingWidget::checkbox("Prefer role badges over custom badges",
                                s->preferTinyRoleBadgesOverCustom)
            ->addTo(layout);
    }

    {
        layout.addTitle("Encryption");
        layout.addDescription("Encrypt your messages");

        SettingWidget::checkbox("Enable message encryption",
                                s->enableMessageEncryption)
            ->addTo(layout);

        SettingWidget::checkbox("Add random spaces after encryption",
                                s->randomSpaces)
            ->addTo(layout);

        SettingWidget::dropdown("Encryption encoding",
                                s->messageEncryptionEncoding)
            ->addTo(layout);

        SettingWidget::lineEdit("Password", s->messagePassword)->addTo(layout);
    }

    layout.addStretch();
}

void TinyrinoPage::onShow()
{
}

}  // namespace chatterino
