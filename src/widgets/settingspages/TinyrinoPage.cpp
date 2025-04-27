#include "widgets/settingspages/TinyrinoPage.hpp"

#include "Application.hpp"
#include "controllers/tinyemotesinstances/TinyemotesInstanceModel.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/LayoutCreator.hpp"
#include "widgets/helper/EditableModelView.hpp"

#include <QHeaderView>
#include <QTableView>

namespace chatterino {

TinyrinoPage::TinyrinoPage()
{
    LayoutCreator<TinyrinoPage> layoutCreator(this);

    auto tabs = layoutCreator.emplace<QTabWidget>();
    this->tabWidget_ = tabs.getElement();

    auto instancesTab = tabs.appendTab(new QVBoxLayout, "TinyEmotes instances");
    {
        instancesTab.emplace<QLabel>(""
                                    "Tinyrino allows you to use emotes "
                                    "from different TinyEmotes instances.\nYou can add an existing "
                                    "instance here or self-host your own!");
        auto linkLabel = instancesTab.emplace<QLabel>("<a href='https://github.com/ilotterytea/tinyemotes' style='color:#99f'>More info...</a>");
        linkLabel->setOpenExternalLinks(true);

        EditableModelView *view =
            instancesTab
                .emplace<EditableModelView>(
                    (new TinyemotesInstanceModel(nullptr))
                        ->initialized(&getSettings()->tinyemotesInstances))
                .getElement();
        this->view_ = view;

        view->setTitles({"Base URL", "Global emotes", "Channel emotes"});
        view->getTableView()->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Interactive);
        view->getTableView()->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::Stretch);

        QTimer::singleShot(1, [view] {
            view->getTableView()->resizeColumnsToContents();
            view->getTableView()->setColumnWidth(1, 125);
            view->getTableView()->setColumnWidth(2, 125);
        });

        // We can safely ignore this signal connection since we own the view
        std::ignore = view->addButtonPressed.connect([] {
            getSettings()->tinyemotesInstances.append(
                TinyemotesInstance("alright.party", true, true));
        });
    }
}

void TinyrinoPage::onShow()
{
}

}  // namespace chatterino
