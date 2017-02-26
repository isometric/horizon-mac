#include "prj-mgr-app_win.hpp"
#include "prj-mgr-app.hpp"
#include <iostream>
#include "project/project.hpp"
#include "util.hpp"
extern const char *gitversion;

namespace horizon {

	ProjectManagerViewCreate::ProjectManagerViewCreate(const Glib::RefPtr<Gtk::Builder>& builder) {
		builder->get_widget("create_project_name_entry", project_name_entry);
		builder->get_widget("create_project_description_entry", project_description_entry);
		builder->get_widget("create_project_path_chooser", project_path_chooser);
		builder->get_widget("create_project_dir_entry", project_dir_entry);
		builder->get_widget("create_project_pool_combo", project_pool_combo);

		project_name_entry->signal_changed().connect(sigc::mem_fun(this, &ProjectManagerViewCreate::update));
		project_description_entry->signal_changed().connect(sigc::mem_fun(this, &ProjectManagerViewCreate::update));
		project_pool_combo->signal_changed().connect(sigc::mem_fun(this, &ProjectManagerViewCreate::update));
		project_path_chooser->signal_file_set().connect(sigc::mem_fun(this, &ProjectManagerViewCreate::update));
	}

	void ProjectManagerViewCreate::clear() {
		project_name_entry->set_text("");
		project_description_entry->set_text("");
		project_dir_entry->set_text("");
		project_path_chooser->unselect_all();
	}

	void ProjectManagerViewCreate::populate_pool_combo(const Glib::RefPtr<Gtk::Application> &app) {
		auto mapp = Glib::RefPtr<ProjectManagerApplication>::cast_dynamic(app);
		project_pool_combo->remove_all();
		for(const auto &it: mapp->pools) {
			project_pool_combo->append((std::string)it.first, it.second.name);
		}
		project_pool_combo->set_active(0);
	}

	std::pair<bool, std::string> ProjectManagerViewCreate::create() {
		bool r = false;
		std::string s;
		try {
			Project prj(UUID::random());
			prj.name = project_name_entry->get_text();
			prj.base_path = Glib::build_filename(project_path_chooser->get_file()->get_path(), prj.name);
			prj.title = project_description_entry->get_text();
			prj.pool_uuid = static_cast<std::string>(project_pool_combo->get_active_id());
			s = prj.create();
			r = true;
		}
		catch (const std::exception& e) {
			r = false;
			auto top = dynamic_cast<Gtk::Window*>(project_name_entry->get_ancestor(GTK_TYPE_WINDOW));
			Gtk::MessageDialog md(*top,  "Error creating project", false /* use_markup */, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
			md.set_secondary_text(e.what());
			md.run();
		}
		catch (const Glib::Error &e) {
			r = false;
			auto top = dynamic_cast<Gtk::Window*>(project_name_entry->get_ancestor(GTK_TYPE_WINDOW));
			Gtk::MessageDialog md(*top,  "Error creating project", false /* use_markup */, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
			md.set_secondary_text(e.what());
			md.run();
		}
		return {r,s};
	}

	void ProjectManagerViewCreate::update() {
		bool valid = true;
		if(project_name_entry->get_text().size() == 0)
			valid = false;
		if(project_description_entry->get_text().size() == 0)
			valid = false;
		if(project_path_chooser->get_filenames().size() == 0)
			valid = false;
		if(project_pool_combo->get_active_row_number()==-1)
			valid = false;
		if(valid) {
			auto base_path = project_path_chooser->get_file()->get_path();
			auto dir = Glib::build_filename(base_path, project_name_entry->get_text());
			project_dir_entry->set_text(dir);
		}
		else {
			project_dir_entry->set_text("");
		}
		signal_valid_change().emit(valid);
	}

	ProjectManagerProcess::ProjectManagerProcess(ProjectManagerProcess::Type ty,  const std::vector<std::string>& args, const std::vector<std::string>& ienv): type(ty) {
		std::cout << "create proc" << std::endl;
		std::vector<std::string> argv;
		std::vector<std::string> env = ienv;
		auto envs = Glib::listenv();
		for(const auto &it: envs) {
			env.push_back(it+"="+Glib::getenv(it));
		}
		auto exe_dir = get_exe_dir();
		auto imp_exe = Glib::build_filename(exe_dir, "horizon-imp");
		switch(type) {
			case ProjectManagerProcess::Type::IMP_SCHEMATIC :
				argv.push_back(imp_exe);
				argv.push_back("-c");
				argv.insert(argv.end(), args.begin(), args.end());
			break;
			case ProjectManagerProcess::Type::IMP_BOARD :
				argv.push_back(imp_exe);
				argv.push_back("-b");
				argv.insert(argv.end(), args.begin(), args.end());
			break;
			default:;
		}
		proc = std::make_unique<EditorProcess>(argv, env);
	}

	ProjectManagerViewProject::ProjectManagerViewProject(const Glib::RefPtr<Gtk::Builder>& builder, ProjectManagerAppWindow *w):win(w) {
		builder->get_widget("button_top_schematic", button_top_schematic);
		builder->get_widget("button_board", button_board);
		builder->get_widget("entry_project_title", entry_project_title);
		builder->get_widget("label_pool_name", label_pool_name);
		builder->get_widget("info_bar", info_bar);
		builder->get_widget("info_bar_label", info_bar_label);

		info_bar->hide();
		info_bar->signal_response().connect([this](int resp){if(resp == Gtk::RESPONSE_CLOSE) info_bar->hide();});

		button_top_schematic->signal_clicked().connect(sigc::mem_fun(this, &ProjectManagerViewProject::handle_button_top_schematic));
		button_board->signal_clicked().connect(sigc::mem_fun(this, &ProjectManagerViewProject::handle_button_board));
	}

	void ProjectManagerViewProject::handle_button_top_schematic() {
		auto prj = win->project.get();
		auto top_block = std::find_if(prj->blocks.begin(), prj->blocks.end(), [](const auto &a){return a.second.is_top;});
		if(top_block != prj->blocks.end()) {
			std::vector<std::string> args = {top_block->second.schematic_filename, top_block->second.block_filename, prj->constraints_filename};
			win->spawn_imp(ProjectManagerProcess::Type::IMP_SCHEMATIC, prj->pool_uuid, args);
		}
	}

	void ProjectManagerViewProject::handle_button_board() {
		auto prj = win->project.get();
		auto top_block = std::find_if(prj->blocks.begin(), prj->blocks.end(), [](const auto &a){return a.second.is_top;});
		if(top_block != prj->blocks.end()) {
			std::vector<std::string> args = {prj->board_filename, top_block->second.block_filename, prj->constraints_filename, prj->vias_directory};
			win->spawn_imp(ProjectManagerProcess::Type::IMP_BOARD, prj->pool_uuid, args);
		}
	}

	void ProjectManagerAppWindow::spawn_imp(ProjectManagerProcess::Type type, const UUID &pool_uuid, const std::vector<std::string> &args) {
		auto app = Glib::RefPtr<ProjectManagerApplication>::cast_dynamic(get_application());
		if(processes.count(args.at(0)) == 0) { //need to launch imp
			auto pool_path = app->pools.at(pool_uuid).path;
			auto ep_broadcast = app->get_ep_broadcast();
			std::vector<std::string> env = {"HORIZON_POOL="+pool_path, "HORIZON_EP_BROADCAST="+ep_broadcast};
			std::string filename = args.at(0);
			auto &proc = processes.emplace(std::piecewise_construct, std::forward_as_tuple(filename),
					std::forward_as_tuple(type, args, env)).first->second;

			proc.proc->signal_exited().connect([filename, this](int status) {
				std::cout << "exit stat " << status << std::endl;
				if(status != 0) {
					view_project.info_bar_label->set_text("Editor for '"+filename+"' exited with status "+std::to_string(status));
					view_project.info_bar->show();

					//ugly workaround for making the info bar appear
					auto parent = dynamic_cast<Gtk::Box*>(view_project.info_bar->get_parent());
					parent->child_property_padding(*view_project.info_bar) = 1;
					parent->child_property_padding(*view_project.info_bar) = 0;
				}
				processes.erase(filename);
			});
		}
		else { //present imp
			auto &proc = processes.at(args.at(0));
			auto pid = proc.proc->get_pid();
			app->send_json(pid, {{"op", "present"}});
		}
	}

	bool ProjectManagerAppWindow::check_pools() {
		auto mapp = Glib::RefPtr<ProjectManagerApplication>::cast_dynamic(get_application());
		if(mapp->pools.size()==0){
			Gtk::MessageDialog md(*this,  "No pools set up", false , Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK);
			md.set_secondary_text("You haven't set up any pools, add some in the preferences dialog");
			md.run();
			return false;
		}
		return true;
	}

	ProjectManagerAppWindow::ProjectManagerAppWindow(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& refBuilder):
			Gtk::ApplicationWindow(cobject), builder(refBuilder), view_create(builder), view_project(builder, this) {
		builder->get_widget("stack", stack);
		builder->get_widget("button_open", button_open);
		builder->get_widget("button_close", button_close);
		builder->get_widget("button_new", button_new);
		builder->get_widget("button_create", button_create);
		builder->get_widget("button_cancel", button_cancel);
		builder->get_widget("button_save", button_save);
		builder->get_widget("header", header);
		builder->get_widget("recent_chooser", recent_chooser);
		builder->get_widget("label_gitversion", label_gitversion);
		set_view_mode(ViewMode::OPEN);

		button_open->signal_clicked().connect(sigc::mem_fun(this, &ProjectManagerAppWindow::handle_open));
		button_new->signal_clicked().connect(sigc::mem_fun(this, &ProjectManagerAppWindow::handle_new));
		button_cancel->signal_clicked().connect(sigc::mem_fun(this, &ProjectManagerAppWindow::handle_cancel));
		button_create->signal_clicked().connect(sigc::mem_fun(this, &ProjectManagerAppWindow::handle_create));
		button_close->signal_clicked().connect(sigc::mem_fun(this, &ProjectManagerAppWindow::handle_close));
		button_save->signal_clicked().connect(sigc::mem_fun(this, &ProjectManagerAppWindow::handle_save));
		recent_chooser->signal_item_activated().connect(sigc::mem_fun(this, &ProjectManagerAppWindow::handle_recent));

		view_create.signal_valid_change().connect([this] (bool v) {button_create->set_sensitive(v);});
		label_gitversion->set_label(gitversion);

		set_icon(Gdk::Pixbuf::create_from_resource("/net/carrotIndustries/horizon/icon.svg"));

		/*
		 * 	auto mapp = Glib::RefPtr<ProjectManagerApplication>::cast_dynamic(get_application());
		if(mapp && mapp->pools.size()==0){
			Gtk::MessageDialog md(*this,  "No pools set up", false , Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK);
			md.set_secondary_text("You haven't set up any pools, add some in the preferences dialog");
			md.run();
		}
		 */
		Glib::signal_idle().connect_once([this]{check_pools();});
	}

	void ProjectManagerAppWindow::handle_new() {
		if(!check_pools())
			return;
		set_view_mode(ViewMode::CREATE);
	}

	void ProjectManagerAppWindow::handle_cancel() {
		view_create.clear();
		set_view_mode(ViewMode::OPEN);
	}

	void ProjectManagerAppWindow::handle_create() {
		auto r = view_create.create();
		if(r.first) {
			view_create.clear();
			open_file_view(Gio::File::create_for_path(r.second));
		}
	}

	void ProjectManagerAppWindow::handle_recent() {
		auto file = Gio::File::create_for_uri(recent_chooser->get_current_uri());
		open_file_view(file);
	}

	void ProjectManagerAppWindow::handle_open() {
		Gtk::FileChooserDialog fc(*this, "Open Project", Gtk::FILE_CHOOSER_ACTION_OPEN);
		fc.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
		fc.add_button("_Open", Gtk::RESPONSE_ACCEPT);
		auto filter= Gtk::FileFilter::create();
		filter->set_name("Horizon projects");
		filter->add_pattern("*.hprj");
		fc.add_filter(filter);

		if(fc.run()==Gtk::RESPONSE_ACCEPT) {
			auto file = fc.get_file();
			open_file_view(file);
		}
	}

	void ProjectManagerAppWindow::handle_save() {
		if(project) {
			project->title = view_project.entry_project_title->get_text();
			header->set_subtitle(project->title);
			save_json_to_file(project_filename, project->serialize());
			auto app = Glib::RefPtr<ProjectManagerApplication>::cast_dynamic(get_application());
			app->send_json(0, {{"op", "save"}});
		}
	}

	void ProjectManagerAppWindow::handle_close() {
		close_project();
	}

	bool ProjectManagerAppWindow::close_project() {
		if(!project)
			return true;
		if(processes.size()) {
			Gtk::MessageDialog md(*this,  "Can't close right now", false /* use_markup */, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
			md.set_secondary_text("Close all running editors first");
			md.run();
			return false;
		}

		project.reset();
		set_view_mode(ViewMode::OPEN);
		return true;
	}

	void ProjectManagerAppWindow::set_view_mode(ViewMode mode) {
		button_open->hide();
		button_close->hide();
		button_new->hide();
		button_cancel->hide();
		button_create->hide();
		button_save->hide();
		header->set_show_close_button(true);
		header->set_subtitle("");

		switch(mode) {
			case ViewMode::OPEN:
				stack->set_visible_child("open");
				button_open->show();
				button_new->show();
			break;

			case ViewMode::PROJECT:
				stack->set_visible_child("project");
				button_close->show();
				button_save->show();
			break;

			case ViewMode::CREATE :
				stack->set_visible_child("create");
				header->set_show_close_button(false);
				button_cancel->show();
				button_create->show();
				view_create.populate_pool_combo(get_application());
			break;
		}
	}

	ProjectManagerAppWindow* ProjectManagerAppWindow::create() {
		// Load the Builder file and instantiate its widgets.
		auto refBuilder = Gtk::Builder::create_from_resource("/net/carrotIndustries/horizon/prj-mgr/window.ui");

		ProjectManagerAppWindow* window = nullptr;
		refBuilder->get_widget_derived("app_window", window);

		if (!window)
			throw std::runtime_error("No \"app_window\" object in window.ui");
		return window;
	}

	void ProjectManagerAppWindow::open_file_view(const Glib::RefPtr<Gio::File>& file) {
		auto path = file->get_path();
		if(project) {
			return;
		}
		try {
			project = std::make_unique<Project>(Project::new_from_file(path));
			project_filename = path;
			set_view_mode(ViewMode::PROJECT);
			header->set_subtitle(project->title);
			view_project.entry_project_title->set_text(project->title);
			auto app = Glib::RefPtr<ProjectManagerApplication>::cast_dynamic(get_application());
			view_project.label_pool_name->set_text(app->pools.at(project->pool_uuid).name);
		}
		catch (const std::exception& e) {
			Gtk::MessageDialog md(*this,  "Error opening project", false /* use_markup */, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
			md.set_secondary_text(e.what());
			md.run();
			project.reset();
			set_view_mode(ViewMode::OPEN);
		}

	}

	bool ProjectManagerAppWindow::on_delete_event(GdkEventAny *ev) {
		//returning false will destroy the window
		std::cout << "close" << std::endl;
		return !close_project();
	}
}
