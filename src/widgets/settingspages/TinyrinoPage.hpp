#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

class QVBoxLayout;

namespace chatterino {

class EditableModelView;

class TinyrinoPage : public SettingsPage
{
public:
    TinyrinoPage();

    void onShow() final;

private:
    QCheckBox *createCheckBox(const QString &text,
                              pajlada::Settings::Setting<bool> &setting,
                              const QString &toolTipText);
    QTabWidget *tabWidget_;
    EditableModelView *view_;
};

}  // namespace chatterino
