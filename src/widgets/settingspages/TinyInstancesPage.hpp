#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

class QVBoxLayout;

namespace chatterino {

class EditableModelView;

class TinyInstancesPage : public SettingsPage
{
public:
    TinyInstancesPage();

    void onShow() final;

private:
    QTabWidget *tabWidget_;
    EditableModelView *view_;
};

}  // namespace chatterino
