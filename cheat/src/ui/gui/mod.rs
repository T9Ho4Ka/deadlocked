use std::time::Duration;

use egui::Ui;
use shared::Data;

use crate::{
    config::{aim::WeaponConfig, write_config},
    message::{GameMessage, GameStatus, RadarMessage},
    ui::{
        app::{App, AppState},
        color::Colors,
        gui::{
            aimbot::AimbotTab,
            helpers::{open_url, text_settings_popup},
        },
        theme,
        window_context::WindowContext,
    },
    update::UpdateStatus,
};

pub mod aimbot;
mod application;
mod config;
mod grenade;
mod helpers;
mod hud;
mod player;
mod radar;
mod r#unsafe;

#[derive(Clone, Copy, PartialEq)]
pub enum Tab {
    Aimbot,
    Player,
    Hud,
    Grenades,
    Unsafe,
    Radar,
    Config,
    Application,
}

const TABS: [(Tab, &str); 8] = [
    (Tab::Aimbot, "Aimbot"),
    (Tab::Player, "Player"),
    (Tab::Hud, "Hud"),
    (Tab::Grenades, "Grenades"),
    (Tab::Unsafe, "Unsafe"),
    (Tab::Radar, "Radar"),
    (Tab::Config, "Config"),
    (Tab::Application, "Application"),
];

const SIDEBAR_WIDTH: f32 = 176.0;

impl AppState {
    pub fn send_config_game(&self) {
        self.send_message_game(GameMessage(Box::new(self.config.clone())));
        self.save();
    }

    pub fn send_message_game(&self, message: GameMessage) {
        if self.channel_game.send(message).is_err() {
            std::process::exit(1);
        }
    }

    pub fn send_config_radar(&self) {
        self.send_message_radar(RadarMessage::Config {
            config: self.config.radar.clone(),
            uuid: self.app_config.radar_uuid,
        });
        self.save();
    }

    pub fn send_message_radar(&self, message: RadarMessage) {
        if self.channel_radar.send(message).is_err() {
            std::process::exit(1);
        }
    }

    fn save(&self) {
        write_config(&self.config, &self.current_config);
    }

    pub(crate) fn gui(&mut self, ui: &mut Ui) {
        ui.ctx().set_pixels_per_point(self.display_scale);

        let theme = self.config.theme.clone();
        let palette = theme.palette();
        let accent = self.config.accent_color;

        theme::paint_gradient(ui.painter(), ui.max_rect(), &theme);

        let panel_frame = egui::Frame::NONE
            .fill(theme.panel_fill(palette.base))
            .inner_margin(egui::Margin::symmetric(10, 10));

        egui::Panel::left("sidebar")
            .resizable(false)
            .exact_size(SIDEBAR_WIDTH)
            .frame(panel_frame)
            .show(ui, |ui| {
                // header and footer are panels of their own so they reserve their height
                // up front. a bottom_up layout would instead draw upwards from the bottom
                // edge and run over the tab list as soon as the window gets short.
                egui::Panel::top("sidebar_header")
                    .frame(egui::Frame::NONE)
                    .show(ui, |ui| {
                        ui.label(
                            egui::RichText::new("deadlocked")
                                .size(21.0)
                                .strong()
                                .color(accent),
                        );
                        ui.add_space(2.0);
                        ui.label(
                            egui::RichText::new(concat!("v", env!("CARGO_PKG_VERSION")))
                                .size(12.0)
                                .color(palette.subtext),
                        );
                        ui.add_space(8.0);
                        ui.separator();
                    });

                egui::Panel::bottom("sidebar_footer")
                    .frame(egui::Frame::NONE)
                    .show(ui, |ui| {
                        ui.separator();
                        ui.add_space(4.0);

                        let status_color = match self.game_status {
                            GameStatus::Working => Colors::GREEN,
                            GameStatus::NotStarted => Colors::YELLOW,
                        };
                        ui.horizontal(|ui| {
                            let (rect, _) =
                                ui.allocate_exact_size(egui::vec2(8.0, 8.0), egui::Sense::hover());
                            ui.painter().circle_filled(rect.center(), 4.0, status_color);
                            ui.label(
                                egui::RichText::new(format!("{}", self.game_status))
                                    .size(13.0)
                                    .color(status_color),
                            );
                        });

                        let frame_avg = if self.frame_times.is_empty() {
                            0.0f32
                        } else {
                            let frame_sum =
                                self.frame_times.iter().sum::<Duration>().as_secs_f32() * 1000.0;
                            frame_sum / self.frame_times.len() as f32
                        };
                        ui.label(
                            egui::RichText::new(format!("{frame_avg:.1} ms"))
                                .size(13.0)
                                .color(palette.subtext),
                        );

                        ui.add_space(4.0);
                        if ui
                            .add_sized(
                                [ui.available_width(), 28.0],
                                egui::Button::new("Report Issue"),
                            )
                            .clicked()
                        {
                            open_url("https://github.com/avitran0/deadlocked/issues");
                        }
                    });

                // whatever height is left belongs to the tabs, and they scroll if even
                // that is not enough
                ui.add_space(6.0);
                egui::ScrollArea::vertical()
                    .id_salt("sidebar_tabs")
                    .auto_shrink([false, false])
                    .show(ui, |ui| {
                        let tab_width = ui.available_width();
                        for (tab, name) in TABS {
                            let selected = self.current_tab == tab;
                            if ui
                                .add_sized(
                                    [tab_width, 30.0],
                                    egui::Button::selectable(selected, name).corner_radius(
                                        egui::CornerRadius::same(theme.corner_radius),
                                    ),
                                )
                                .clicked()
                            {
                                self.current_tab = tab;
                            }
                        }
                    });
            });

        let central_frame = egui::Frame::NONE
            .fill(theme.panel_fill(palette.base))
            .inner_margin(egui::Margin::symmetric(12, 10));

        egui::CentralPanel::default()
            .frame(central_frame)
            .show(ui, |ui| match self.current_tab {
                Tab::Aimbot => self.aimbot_settings(ui),
                Tab::Player => self.player_settings(ui),
                Tab::Hud => self.hud_settings(ui),
                Tab::Grenades => self.grenade_settings(ui),
                Tab::Unsafe => self.unsafe_settings(ui),
                Tab::Radar => self.radar_settings(ui),
                Tab::Config => self.config_settings(ui),
                Tab::Application => self.application_settings(ui),
            });

        self.render_text_popups(ui);

        if self.update_popup {
            let mut close = false;
            egui::Window::new("Update Available")
                .id(egui::Id::new("update_popup"))
                .collapsible(false)
                .resizable(false)
                .anchor(egui::Align2::CENTER_CENTER, egui::vec2(0.0, 0.0))
                .show(ui.ctx(), |ui| {
                    if let UpdateStatus::Available { version, url } = &self.update_status {
                        ui.label(
                            egui::RichText::new(format!("Update {version} available!"))
                                .color(Colors::YELLOW)
                                .size(18.0),
                        );
                        ui.separator();
                        ui.label("A new version of deadlocked is ready to download.");
                        ui.add_space(8.0);
                        ui.horizontal(|ui| {
                            if ui.button("Download").clicked() {
                                open_url(url);
                            }
                            if ui.button("Dismiss").clicked() {
                                close = true;
                            }
                        });
                    }
                });
            if close {
                self.update_popup = false;
            }
        }

        if self.omarchy_popup {
            let mut close = false;
            egui::Window::new("Omarchy Warning")
                .id(egui::Id::new("omarchy_warning_popup"))
                .collapsible(false)
                .resizable(false)
                .anchor(egui::Align2::CENTER_CENTER, egui::vec2(0.0, 0.0))
                .show(ui.ctx(), |ui| {
                    ui.label(
                        egui::RichText::new(
                            "Warning: Omarchy is part of the normalization of fascism in open source.",
                        )
                        .color(Colors::YELLOW)
                        .size(18.0),
                    );
                    ui.add_space(8.0);
                    ui.label(
                        "DHH's Linux distribution is backed by the Omacom Foundation and a network of wealthy tech executives and companies. The article documents the white-nationalist and far-right politics behind that funding.",
                    );
                    ui.add_space(8.0);
                    ui.label("I would greatly suggest picking another distro.");
                    ui.add_space(8.0);
                    ui.label("Read the article for the full context:");
                    if ui
                        .link("Normalized Fascism in Open Source: $12 Million Given to DHH")
                        .clicked()
                    {
                        open_url(
                            "https://brennan.day/normalized-fascism-in-open-source-12-million-given-to-dhh/",
                        );
                    }
                    ui.separator();
                    if ui.button("Close").clicked() {
                        close = true;
                    }
                });
            if close {
                self.omarchy_popup = false;
            }
        }
    }

    fn weapon_config(&mut self) -> &mut WeaponConfig {
        if self.aimbot_tab == AimbotTab::Weapon {
            self.config
                .aim
                .weapons
                .get_mut(&self.aimbot_weapon)
                .unwrap()
        } else {
            &mut self.config.aim.global
        }
    }

    fn render_text_popups(&mut self, ui: &mut Ui) {
        let text = &mut self.config.hud.overlay_text;
        let mut changed = false;
        changed |= text_settings_popup(
            ui,
            "Status Text",
            &mut text.status_text,
            &mut self.text_popup,
            "status_text",
        );
        changed |= text_settings_popup(
            ui,
            "Player Name",
            &mut text.player_name,
            &mut self.text_popup,
            "player_name",
        );
        changed |= text_settings_popup(
            ui,
            "Player Tags",
            &mut text.player_tags,
            &mut self.text_popup,
            "player_tags",
        );
        changed |= text_settings_popup(
            ui,
            "Weapon Icon",
            &mut text.weapon_icon,
            &mut self.text_popup,
            "weapon_icon",
        );
        changed |= text_settings_popup(
            ui,
            "Ammo",
            &mut text.ammo_text,
            &mut self.text_popup,
            "ammo_text",
        );
        changed |= text_settings_popup(
            ui,
            "Weapon Name",
            &mut text.weapon_name,
            &mut self.text_popup,
            "weapon_name",
        );
        changed |= text_settings_popup(
            ui,
            "Bomb Timer",
            &mut text.bomb_timer,
            &mut self.text_popup,
            "bomb_timer",
        );
        changed |= text_settings_popup(
            ui,
            "Grenade Name",
            &mut text.grenade_name,
            &mut self.text_popup,
            "grenade_name",
        );
        changed |= text_settings_popup(
            ui,
            "Grenade Lineup",
            &mut text.grenade_lineup,
            &mut self.text_popup,
            "grenade_lineup",
        );
        changed |= text_settings_popup(
            ui,
            "Keybind List",
            &mut text.keybind_list,
            &mut self.text_popup,
            "keybind_list",
        );
        changed |= text_settings_popup(
            ui,
            "Spectator List",
            &mut text.spectator_list,
            &mut self.text_popup,
            "spectator_list",
        );
        if changed {
            self.send_config_game();
        }
    }
}

impl App {
    pub(crate) fn update_overlay_window(overlay: &WindowContext, data: &Data) {
        use winit::dpi::PhysicalPosition;
        let position =
            PhysicalPosition::new(data.window_position.x as i32, data.window_position.y as i32);
        if !match overlay.window().outer_position() {
            Ok(pos) => pos == position,
            Err(_) => false,
        } {
            overlay.window().set_outer_position(position);
        }

        let size = winit::dpi::PhysicalSize::new(
            data.window_size.x.max(1.0) as u32,
            data.window_size.y.max(1.0) as u32,
        );
        if overlay.window().inner_size() != size {
            let _ = overlay.window().request_inner_size(size);
        }
    }
}
