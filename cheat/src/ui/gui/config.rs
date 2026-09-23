use egui::{Align, Button, Ui};

use crate::{
    config::{
        BASE_PATH, CONFIG_PATH, Config, application::write_app_config, available_configs,
        delete_config, parse_config, write_config,
    },
    ui::{
        app::AppState,
        color::Colors,
        grenades::read_grenades,
        gui::helpers::{collapsing_open, combo_box, open_url, scroll},
        theme::{self, Theme},
    },
};

impl AppState {
    pub fn config_settings(&mut self, ui: &mut Ui) {
        ui.columns(2, |cols| {
            let left = &mut cols[0];
            scroll(left, "config_left", |left| self.config_left(left));

            let right = &mut cols[1];

            collapsing_open(right, "Configs", |right| {
                if right.button("Refresh").clicked() {
                    self.available_configs = available_configs();
                    self.grenades = read_grenades();
                }

                right.horizontal(|right| {
                    if right.button("+").clicked() && !self.new_config_name.is_empty() {
                        if !self.new_config_name.ends_with(".toml") {
                            self.new_config_name.push_str(".toml");
                        }
                        let path = CONFIG_PATH.join(&self.new_config_name);
                        write_config(&self.config, &path);
                        self.new_config_name.clear();
                        self.current_config = path;
                        self.app_config.config_name = self
                            .current_config
                            .file_name()
                            .unwrap()
                            .to_string_lossy()
                            .into_owned();
                        write_app_config(&self.app_config);
                        self.available_configs = available_configs();
                    }
                    right.text_edit_singleline(&mut self.new_config_name);
                });

                scroll(right, "config_right", |right| self.config_right(right));
            });
        });
    }

    fn config_left(&mut self, ui: &mut Ui) {
        collapsing_open(ui, "Config", |ui| {
            if ui.button("Reset").clicked() {
                self.config = Config::default();
                self.apply_theme(ui.ctx());
                self.send_config_game();
                utils::info!("loaded default config");
            }

            if ui.button("Config Folder").clicked() {
                let url = format!("file://{}", BASE_PATH.display());
                open_url(&url);
            }
        });

        collapsing_open(ui, "Appearance", |ui| {
            let previous = self.config.theme.theme;
            if combo_box(ui, "theme", "Theme", &mut self.config.theme.theme) {
                if self.config.theme.theme == Theme::Custom {
                    // start editing from whatever the user was just looking at
                    self.config.theme.seed_custom(previous);
                } else {
                    // a preset ships its own accent and gradient, adopt them on switch
                    self.config.accent_color = self.config.theme.theme.palette().accent;
                    self.config.theme.sync_gradient();
                }
                self.apply_theme(ui.ctx());
                self.send_config_game();
            }

            egui::ComboBox::new("accent_color", "Accent Color")
                .selected_text(
                    Colors::ACCENT_COLORS
                        .iter()
                        .find(|c| c.1 == self.config.accent_color)
                        .map_or("Custom", |c| c.0),
                )
                .show_ui(ui, |ui| {
                    for (name, color) in Colors::ACCENT_COLORS {
                        if ui
                            .add(
                                Button::selectable(color == self.config.accent_color, name)
                                    .fill(color),
                            )
                            .clicked()
                        {
                            self.config.accent_color = color;
                            self.apply_theme(ui.ctx());
                            self.send_config_game();
                        }
                    }
                });

            ui.horizontal(|ui| {
                if ui
                    .add(egui::Slider::new(
                        &mut self.config.theme.corner_radius,
                        0..=16,
                    ))
                    .changed()
                {
                    self.apply_theme(ui.ctx());
                    self.send_config_game();
                }
                ui.label("Corner Radius");
            });

            if ui
                .checkbox(&mut self.config.theme.shadows, "Shadows")
                .changed()
            {
                self.apply_theme(ui.ctx());
                self.send_config_game();
            }
        });

        collapsing_open(ui, "Text", |ui| {
            let mut changed = false;

            ui.horizontal(|ui| {
                changed |= ui
                    .add(
                        egui::Slider::new(&mut self.config.theme.text_scale, 0.8..=1.5)
                            .fixed_decimals(2),
                    )
                    .changed();
                ui.label("Text Size");
            })
            .response
            .on_hover_text("scales every text size at once, headings included");

            ui.horizontal(|ui| {
                changed |= ui
                    .add(
                        egui::Slider::new(&mut self.config.theme.text_contrast, 0.0..=1.0)
                            .fixed_decimals(2),
                    )
                    .changed();
                ui.label("Secondary Contrast");
            })
            .response
            .on_hover_text("brightens dimmed text, 1.0 makes it as bright as labels");

            if changed {
                self.apply_theme(ui.ctx());
                self.send_config_game();
            }
        });

        if self.config.theme.theme == Theme::Custom {
            collapsing_open(ui, "Custom Colors", |ui| {
                let mut changed = false;
                let custom = &mut self.config.theme.custom;

                changed |= ui
                    .checkbox(&mut custom.dark, "Dark Theme")
                    .on_hover_text("tells egui which way round the palette goes")
                    .changed();

                let mut color_row =
                    |ui: &mut Ui, label: &str, color: &mut egui::Color32, hint: &str| {
                        ui.horizontal(|ui| {
                            changed |= ui.color_edit_button_srgba(color).changed();
                            ui.label(label).on_hover_text(hint);
                        });
                    };

                color_row(
                    ui,
                    "Backdrop",
                    &mut custom.backdrop,
                    "text fields, scroll troughs",
                );
                color_row(ui, "Base", &mut custom.base, "panel and window background");
                color_row(
                    ui,
                    "Surface",
                    &mut custom.surface,
                    "idle buttons and controls",
                );
                color_row(
                    ui,
                    "Overlay",
                    &mut custom.overlay,
                    "hovered controls and borders",
                );
                color_row(ui, "Text", &mut custom.text, "primary text");
                color_row(
                    ui,
                    "Subtext",
                    &mut custom.subtext,
                    "hints and idle control text",
                );

                ui.add_space(4.0);
                ui.horizontal(|ui| {
                    for theme in [Theme::Midnight, Theme::Mocha, Theme::Nord, Theme::Daylight] {
                        if ui.button(format!("{theme}")).clicked() {
                            self.config.theme.seed_custom(theme);
                            changed = true;
                        }
                    }
                })
                .response
                .on_hover_text("copy a preset into the custom palette as a starting point");

                if changed {
                    self.apply_theme(ui.ctx());
                    self.send_config_game();
                }
            });
        }

        collapsing_open(ui, "Gradient", |ui| {
            if ui
                .checkbox(&mut self.config.theme.gradient, "Enable Gradient")
                .changed()
            {
                self.apply_theme(ui.ctx());
                self.send_config_game();
            }

            ui.add_enabled_ui(self.config.theme.gradient, |ui| {
                let mut changed = false;

                ui.horizontal(|ui| {
                    changed |= ui
                        .color_edit_button_srgba(&mut self.config.theme.gradient_start)
                        .changed();
                    changed |= ui
                        .color_edit_button_srgba(&mut self.config.theme.gradient_end)
                        .changed();
                    ui.label("Colors");
                });

                changed |= combo_box(
                    ui,
                    "gradient_direction",
                    "Direction",
                    &mut self.config.theme.gradient_direction,
                );

                ui.horizontal(|ui| {
                    changed |= ui
                        .add(
                            egui::Slider::new(&mut self.config.theme.panel_opacity, 0.0..=1.0)
                                .fixed_decimals(2),
                        )
                        .changed();
                    ui.label("Panel Opacity");
                })
                .response
                .on_hover_text("how much of the gradient shows through the panels");

                if ui.button("Reset To Theme").clicked() {
                    self.config.theme.sync_gradient();
                    changed = true;
                }

                if changed {
                    self.apply_theme(ui.ctx());
                    self.send_config_game();
                }
            });
        });
    }

    /// Pushes the current theme into the gui context and, if it exists, the overlay context.
    fn apply_theme(&self, ctx: &egui::Context) {
        theme::apply_to_ctx(ctx, &self.config.theme, self.config.accent_color);
        if let Some(overlay) = &self.overlay_egui {
            theme::apply_to_ctx(overlay, &self.config.theme, self.config.accent_color);
        }
    }

    fn config_right(&mut self, ui: &mut Ui) {
        let mut clicked_config = None;
        let mut delete = None;

        for config in &self.available_configs {
            ui.horizontal(|ui| {
                if ui
                    .add(Button::selectable(
                        *config == self.current_config,
                        config.file_name().unwrap().to_str().unwrap(),
                    ))
                    .clicked()
                {
                    clicked_config = Some(config.clone());
                }
                ui.with_layout(egui::Layout::right_to_left(Align::Center), |ui| {
                    if ui.button("Delete").clicked() {
                        delete = Some(config.clone());
                    }
                });
            });
        }

        if let Some(config_path) = clicked_config {
            self.config = parse_config(&config_path);
            self.current_config = config_path;
            self.app_config.config_name = self
                .current_config
                .file_name()
                .unwrap()
                .to_string_lossy()
                .into_owned();
            write_app_config(&self.app_config);
            self.send_config_game();
            self.apply_theme(ui.ctx());
        }

        if let Some(config) = delete {
            delete_config(&config);
            self.available_configs = available_configs();
            self.current_config = self.available_configs[0].clone();
            self.app_config.config_name = self
                .current_config
                .file_name()
                .unwrap()
                .to_string_lossy()
                .into_owned();
            write_app_config(&self.app_config);
            self.config = parse_config(&self.current_config);
        }
    }
}
