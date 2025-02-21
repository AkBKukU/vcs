#include <QGraphicsProxyWidget>
#include <QMessageBox>
#include <QFileDialog>
#include <QStatusBar>
#include <QMenuBar>
#include <QTimer>
#include <QLabel>
#include <functional>
#include "display/qt/dialogs/filter_graph/filter_graph_node.h"
#include "display/qt/dialogs/filter_graph/filter_node.h"
#include "display/qt/dialogs/filter_graph/input_gate_node.h"
#include "display/qt/dialogs/filter_graph/output_gate_node.h"
#include "display/qt/subclasses/QGraphicsItem_interactible_node_graph_node.h"
#include "display/qt/subclasses/QGraphicsScene_interactible_node_graph.h"
#include "display/qt/subclasses/QMenu_dialog_file_menu.h"
#include "display/qt/subclasses/QFrame_filtergui_for_qt.h"
#include "display/qt/dialogs/filter_graph_dialog.h"
#include "display/qt/persistent_settings.h"
#include "filter/filtergui.h"
#include "filter/abstract_filter.h"
#include "common/command_line/command_line.h"
#include "common/disk/disk.h"
#include "ui_filter_graph_dialog.h"

FilterGraphDialog::FilterGraphDialog(QWidget *parent) :
    VCSBaseDialog(parent),
    ui(new Ui::FilterGraphDialog)
{
    ui->setupUi(this);

    this->set_name("Filter graph");

    this->setWindowFlags(Qt::Window);

    // Construct the status bar.
    {
        auto *const bar = new QStatusBar();
        this->layout()->addWidget(bar);

        auto *const viewScale = new QLabel("Scale: 1.0");

        connect(ui->graphicsView, &InteractibleNodeGraphView::scale_changed, this, [=](const double newScale)
        {
            viewScale->setText(QString("Scale: %1").arg(QString::number(newScale, 'f', 1)));
        });

        connect(ui->graphicsView, &InteractibleNodeGraphView::right_clicked_on_background, this, [=](const QPoint &globalPos)
        {
            this->filtersMenu->popup(globalPos);
        });

        bar->addPermanentWidget(viewScale);
    }

    // Create the dialog's menu bar.
    {
        this->menuBar = new QMenuBar(this);

        // File...
        {
            auto *const file = new DialogFileMenu(this);

            this->menuBar->addMenu(file);

            connect(file, &DialogFileMenu::save, this, [=](const QString &filename)
            {
                this->save_graph_into_file(filename);
            });

            connect(file, &DialogFileMenu::open, this, [=]
            {
                QString filename = QFileDialog::getOpenFileName(this,
                                                                "Select a file containing the filter graph to be loaded", "",
                                                                "Filter graphs (*.vcs-filter-graph);;"
                                                                "All files(*.*)");

                this->load_graph_from_file(filename);
            });

            connect(file, &DialogFileMenu::save_as, this, [=](const QString &originalFilename)
            {
                QString filename = QFileDialog::getSaveFileName(this,
                                                                "Select a file to save the filter graph into",
                                                                originalFilename,
                                                                "Filter graph files (*.vcs-filter-graph);;"
                                                                "All files(*.*)");

                this->save_graph_into_file(filename);
            });

            connect(file, &DialogFileMenu::close, this, [=]
            {
                this->reset_graph(true);
                this->set_data_filename("");
            });
        }

        // Graph...
        {
            QMenu *filterGraphMenu = new QMenu("Graph", this->menuBar);

            QAction *enable = new QAction("Enabled", this->menuBar);
            enable->setCheckable(true);
            enable->setChecked(this->is_enabled());

            connect(this, &VCSBaseDialog::enabled_state_set, this, [=](const bool isEnabled)
            {
                enable->setChecked(isEnabled);
            });

            connect(enable, &QAction::triggered, this, [=]
            {
                this->set_enabled(!this->is_enabled());
            });

            filterGraphMenu->addAction(enable);

            this->menuBar->addMenu(filterGraphMenu);
        }

        // Nodes...
        {
            this->filtersMenu = new QMenu("Filters", this);

            // Insert the names of all available filter types.
            {
                QMenu *enhanceMenu = new QMenu("Enhance", this);
                QMenu *reduceMenu = new QMenu("Reduce", this);
                QMenu *distortMenu = new QMenu("Distort", this);
                QMenu *metaMenu = new QMenu("Information", this);

                auto knownFilterTypes = kf_available_filter_types();

                std::sort(knownFilterTypes.begin(), knownFilterTypes.end(), [](const abstract_filter_c *a, const abstract_filter_c *b)
                {
                    return a->name() < b->name();
                });

                // Add gates.
                for (const auto filter: knownFilterTypes)
                {
                    if ((filter->category() != filter_category_e::input_condition) &&
                        (filter->category() != filter_category_e::output_condition))
                    {
                        continue;
                    }

                    connect(filtersMenu->addAction(QString::fromStdString(filter->name())), &QAction::triggered, this, [=]
                    {
                        this->add_filter_graph_node(filter->uuid());
                    });
                }

                filtersMenu->addSeparator();

                // Add filters.
                {
                    filtersMenu->addMenu(enhanceMenu);
                    filtersMenu->addMenu(reduceMenu);
                    filtersMenu->addMenu(distortMenu);
                    filtersMenu->addMenu(metaMenu);

                    for (const auto filter: knownFilterTypes)
                    {
                        if ((filter->category() == filter_category_e::input_condition) ||
                            (filter->category() == filter_category_e::output_condition))
                        {
                            continue;
                        }

                        auto *const action = new QAction(QString::fromStdString(filter->name()), filtersMenu);

                        switch (filter->category())
                        {
                            case filter_category_e::distort: distortMenu->addAction(action); break;
                            case filter_category_e::enhance: enhanceMenu->addAction(action); break;
                            case filter_category_e::reduce: reduceMenu->addAction(action); break;
                            case filter_category_e::meta: metaMenu->addAction(action); break;
                            default: k_assert(0, "Unknown filter category."); break;
                        }

                        connect(action, &QAction::triggered, this, [=]
                        {
                            this->add_filter_graph_node(filter->uuid());
                        });
                    }
                }
            }

            this->menuBar->addMenu(filtersMenu);
        }

        this->layout()->setMenuBar(this->menuBar);
    }

    // Connect the GUI components to consequences for changing their values.
    {
        connect(this, &VCSBaseDialog::enabled_state_set, this, [=](const bool isEnabled)
        {
            kf_set_filtering_enabled(isEnabled);
            kd_update_output_window_title();
        });

        connect(this, &VCSBaseDialog::data_filename_changed, this, [=](const QString &newFilename)
        {
            // Kludge fix for the filter graph not repainting itself properly when new nodes
            // are loaded in. Let's just force it to do so.
            this->refresh_filter_graph();

            kpers_set_value(INI_GROUP_FILTER_GRAPH, "graph_source_file", newFilename);
        });
    }

    // Create and configure the graphics scene.
    {
        this->graphicsScene = new InteractibleNodeGraph(this);
        this->graphicsScene->setBackgroundBrush(QBrush("transparent"));

        ui->graphicsView->setScene(this->graphicsScene);

        connect(this->graphicsScene, &InteractibleNodeGraph::edgeConnectionAdded, this, [this]
        {
            emit this->data_changed();
            this->recalculate_filter_chains();
        });

        connect(this->graphicsScene, &InteractibleNodeGraph::edgeConnectionRemoved, this, [this]
        {
            emit this->data_changed();
            this->recalculate_filter_chains();
        });

        connect(this->graphicsScene, &InteractibleNodeGraph::nodeRemoved, this, [this](InteractibleNodeGraphNode *const node)
        {
            FilterGraphNode *const filterNode = dynamic_cast<FilterGraphNode*>(node);

            if (filterNode)
            {
                emit this->data_changed();

                if (filterNode->associatedFilter->category() == filter_category_e::input_condition)
                {
                    this->inputGateNodes.erase(std::find(inputGateNodes.begin(), inputGateNodes.end(), filterNode));
                }

                delete filterNode;

                /// TODO: When a node is deleted, recalculate_filter_chains() gets
                /// called quite a few times more than needed - once for each of its
                /// connections, and a last time here.
                this->recalculate_filter_chains();
            }
        });
    }

    // Restore persistent settings.
    this->reset_graph(true);
    {
        this->set_enabled(kpers_value_of(INI_GROUP_OUTPUT, "custom_filtering", kf_is_filtering_enabled()).toBool());
        this->resize(kpers_value_of(INI_GROUP_GEOMETRY, "filter_graph", this->size()).toSize());

        if (kcom_filter_graph_file_name().empty())
        {
            const QString graphSourceFilename = kpers_value_of(INI_GROUP_FILTER_GRAPH, "graph_source_file", QString("")).toString();
            kcom_override_filter_graph_file_name(graphSourceFilename.toStdString());
        }
    }

    return;
}

FilterGraphDialog::~FilterGraphDialog()
{
    // Save persistent settings.
    {
        kpers_set_value(INI_GROUP_OUTPUT, "custom_filtering", this->is_enabled());
        kpers_set_value(INI_GROUP_GEOMETRY, "filter_graph", this->size());
    }

    delete ui;

    return;
}

void FilterGraphDialog::reset_graph(const bool autoAccept)
{
    if (autoAccept ||
        QMessageBox::question(this,
                              "Create a new graph?",
                              "Any unsaved changes in the current graph will be lost. Continue?") == QMessageBox::Yes)
    {
        this->clear_filter_graph();
    }

    return;
}

bool FilterGraphDialog::load_graph_from_file(const QString &filename)
{
    if (filename.isEmpty())
    {
        return false;
    }

    const auto loadedAbstractNodes = kdisk_load_filter_graph(filename.toStdString());

    if (!loadedAbstractNodes.empty())
    {
        this->clear_filter_graph();

        // Add the loaded nodes to the filter graph.
        {
            std::vector<FilterGraphNode*> addedNodes;

            for (const auto &abstractNode: loadedAbstractNodes)
            {
                FilterGraphNode *const node = this->add_filter_graph_node(abstractNode.typeUuid, abstractNode.parameters);
                k_assert(node, "Failed to create a filter graph node.");

                node->setPos(abstractNode.position.first, abstractNode.position.second);
                node->set_background_color(QString::fromStdString(abstractNode.backgroundColor));
                node->set_enabled(abstractNode.isEnabled);

                addedNodes.push_back(node);
            }

            // Connect the nodes to each other.
            for (unsigned i = 0; i < loadedAbstractNodes.size(); i++)
            {
                if (loadedAbstractNodes[i].connectedTo.empty())
                {
                    continue;
                }

                node_edge_s *const srcEdge = addedNodes.at(i)->output_edge();
                k_assert(srcEdge, "Invalid source edge for connecting filter nodes.");

                for (const auto dstNodeIdx: loadedAbstractNodes.at(i).connectedTo)
                {
                    node_edge_s *const dstEdge = addedNodes.at(dstNodeIdx)->input_edge();
                    k_assert(dstEdge, "Invalid destination or target edge for connecting filter nodes.");

                    srcEdge->connect_to(dstEdge);
                }
            }
        }

        this->set_data_filename(filename);

        return true;
    }
    else
    {
        this->set_data_filename("");
        return false;
    }
}

void FilterGraphDialog::save_graph_into_file(QString filename)
{
    if (filename.isEmpty())
    {
        return;
    }

    if (QFileInfo(filename).suffix().isEmpty())
    {
        filename += ".vcs-filter-graph";
    }

    std::vector<FilterGraphNode*> filterNodes;
    {
        for (auto node: this->graphicsScene->items())
        {
            const auto filterNode = dynamic_cast<FilterGraphNode*>(node);

            if (filterNode)
            {
                filterNodes.push_back(filterNode);
            }
        }
    }

    if (kdisk_save_filter_graph(filterNodes, filename.toStdString()))
    {
        this->set_data_filename(filename);
    }

    return;
}

// Adds a new instance of the given filter type into the node graph. Returns a
// pointer to the new node.
FilterGraphNode* FilterGraphDialog::add_filter_graph_node(const std::string &filterTypeUuid,
                                                          const std::vector<std::pair<unsigned, double>> &initialParamValues)
{
    abstract_filter_c *const filter = kf_create_filter_instance(filterTypeUuid, initialParamValues);
    k_assert(filter, "Failed to create a new filter node.");

    FilterGUIForQt *const guiWidget = new FilterGUIForQt(filter);

    const unsigned filterWidgetWidth = (guiWidget->width() + 10);
    const unsigned filterWidgetHeight = (guiWidget->height() + 29);
    const QString nodeTitle = QString("%1. %2").arg(this->numNodesAdded+1).arg(QString::fromStdString(filter->name()));

    FilterGraphNode *newNode = nullptr;

    // Initialize the node.
    {
        switch (filter->category())
        {
            case filter_category_e::input_condition: newNode = new InputGateNode(nodeTitle, filterWidgetWidth, filterWidgetHeight); break;
            case filter_category_e::output_condition: newNode = new OutputGateNode(nodeTitle, filterWidgetWidth, filterWidgetHeight); break;
            default: newNode = new FilterNode(nodeTitle, filterWidgetWidth, filterWidgetHeight); break;
        }

        newNode->associatedFilter = filter;
        newNode->setFlags(newNode->flags() | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsSelectable);
        this->graphicsScene->addItem(newNode);

        QGraphicsProxyWidget* nodeWidgetProxy = new QGraphicsProxyWidget(newNode);
        nodeWidgetProxy->setWidget(guiWidget);
        nodeWidgetProxy->widget()->move(2, 27);

        if (filter->category() == filter_category_e::input_condition)
        {
            this->inputGateNodes.push_back(newNode);
        }

        newNode->moveBy(rand()%20, rand()%20);
    }

    // Make sure the node shows up top.
    {
        double maxZ = 0;

        const auto sceneItems = this->graphicsScene->items();

        for (auto item: sceneItems)
        {
            if (item->zValue() > maxZ)
            {
                maxZ = item->zValue();
            }
        }

        newNode->setZValue(maxZ + 1);

        ui->graphicsView->centerOn(newNode);
    }

    connect(guiWidget, &FilterGUIForQt::parameter_changed, this, [this]
    {
        this->data_changed();
    });

    connect(newNode, &FilterGraphNode::background_color_changed, this, [this]
    {
        this->data_changed();
    });

    connect(newNode, &FilterGraphNode::enabled_state_set, this, [this]
    {
        this->data_changed();
    });

    this->numNodesAdded++;
    emit this->data_changed();

    return newNode;
}

// Visit each node in the graph and while doing so, group together such chains of
// filters that run from an input gate through one or more filters into an output
// gate. The chains will then be submitted to the filter handler for use in applying
// the filters to captured frames.
void FilterGraphDialog::recalculate_filter_chains(void)
{
    kf_unregister_all_filter_chains();

    const std::function<void(FilterGraphNode *const, std::vector<abstract_filter_c*>)> traverse_filter_node =
          [&](FilterGraphNode *const node, std::vector<abstract_filter_c*> accumulatedFilterChain)
    {
        k_assert((node && node->associatedFilter), "Trying to visit an invalid node.");

        if (std::find(accumulatedFilterChain.begin(),
                      accumulatedFilterChain.end(),
                      node->associatedFilter) != accumulatedFilterChain.end())
        {
            kd_show_headless_error_message("VCS detected a potential infinite loop",
                                           "One or more filter chains in the filter graph are connected in a loop "
                                           "(a node's output connects back to its input).\n\nChains containing an "
                                           "infinite loop will remain unusable until the loop is disconnected.");

            return;
        }

        if (node->is_enabled())
        {
            accumulatedFilterChain.push_back(node->associatedFilter);
        }

        if (node->associatedFilter->category() == filter_category_e::output_condition)
        {
            kf_register_filter_chain(accumulatedFilterChain);
            return;
        }

        // NOTE: This assumes that each node in the graph only has one output edge.
        for (auto outgoing: node->output_edge()->connectedTo)
        {
            traverse_filter_node(dynamic_cast<FilterGraphNode*>(outgoing->parentNode), accumulatedFilterChain);
        }

        return;
    };

    for (auto inputGate: this->inputGateNodes)
    {
        traverse_filter_node(inputGate, {});
    }

    return;
}

void FilterGraphDialog::clear_filter_graph(void)
{
    kf_unregister_all_filter_chains();
    this->graphicsScene->reset_scene();
    this->inputGateNodes.clear();
    this->numNodesAdded = 0;

    return;
}

void FilterGraphDialog::refresh_filter_graph(void)
{
    this->graphicsScene->update_scene_connections();

    return;
}
