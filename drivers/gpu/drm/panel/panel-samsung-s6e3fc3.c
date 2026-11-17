// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2026, Jiro <ktgithub28@gmail.com>
 *
 * DRM driver for Samsung AMOLED S6E3FC3 / S6E3HAB FHD+ Command Mode DSI panels
 * found in Samsung Galaxy A52s 5G (a52sxq), Galaxy A73 5G (a73xq), and Galaxy M52 5G (m52xq).
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/backlight.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct s6e3fc3_panel {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *supply_vddio;
	struct regulator *supply_vci;
	struct gpio_desc *reset_gpio;
	const struct drm_display_mode *mode;
	bool prepared;
};

static inline struct s6e3fc3_panel *to_s6e3fc3_panel(struct drm_panel *panel)
{
	return container_of(panel, struct s6e3fc3_panel, panel);
}

static void s6e3fc3_panel_reset(struct s6e3fc3_panel *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int s6e3fc3_panel_on(struct s6e3fc3_panel *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(30);

	/* Unlock Level 1 Key */
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0x5a, 0x5a);

	/* Tear On Mode */
	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}

	/* Vendor-specific tuning */
	mipi_dsi_dcs_write_seq(dsi, 0xe5, 0x15);
	mipi_dsi_dcs_write_seq(dsi, 0xed, 0x44, 0x4c, 0x20);

	/* Lock Level 1 Key */
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	msleep(90);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}

	return 0;
}

static int s6e3fc3_panel_off(struct s6e3fc3_panel *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	/* Unlock Level 0 Key */
	mipi_dsi_dcs_write_seq(dsi, 0x9f, 0xa5, 0xa5);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		dev_err(dev, "Failed to set display off: %d\n", ret);
	msleep(20);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0)
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
	msleep(120);

	/* Lock Level 0 Key */
	mipi_dsi_dcs_write_seq(dsi, 0x9f, 0x5a, 0x5a);

	return 0;
}

static int s6e3fc3_panel_prepare(struct drm_panel *panel)
{
	struct s6e3fc3_panel *ctx = to_s6e3fc3_panel(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	if (ctx->supply_vci) {
		ret = regulator_enable(ctx->supply_vci);
		if (ret < 0) {
			dev_err(dev, "Failed to enable vci regulator: %d\n", ret);
			return ret;
		}
	}

	if (ctx->supply_vddio) {
		ret = regulator_enable(ctx->supply_vddio);
		if (ret < 0) {
			dev_err(dev, "Failed to enable vddio regulator: %d\n", ret);
			if (ctx->supply_vci)
				regulator_disable(ctx->supply_vci);
			return ret;
		}
	}

	s6e3fc3_panel_reset(ctx);

	ret = s6e3fc3_panel_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		if (ctx->supply_vddio)
			regulator_disable(ctx->supply_vddio);
		if (ctx->supply_vci)
			regulator_disable(ctx->supply_vci);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int s6e3fc3_panel_unprepare(struct drm_panel *panel)
{
	struct s6e3fc3_panel *ctx = to_s6e3fc3_panel(panel);

	if (!ctx->prepared)
		return 0;

	s6e3fc3_panel_off(ctx);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	if (ctx->supply_vddio)
		regulator_disable(ctx->supply_vddio);
	if (ctx->supply_vci)
		regulator_disable(ctx->supply_vci);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode ams646yd01_mode_120hz = {
	.clock = 386646,
	.hdisplay = 1080,
	.hsync_start = 1080 + 80,
	.hsync_end = 1080 + 80 + 84,
	.htotal = 1080 + 80 + 84 + 88,
	.vdisplay = 2400,
	.vsync_start = 2400 + 15,
	.vsync_end = 2400 + 15 + 2,
	.vtotal = 2400 + 15 + 2 + 2,
	.width_mm = 67,
	.height_mm = 150,
	.flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
};

static const struct drm_display_mode ams646yd01_mode_60hz = {
	.clock = 193406,
	.hdisplay = 1080,
	.hsync_start = 1080 + 80,
	.hsync_end = 1080 + 80 + 84,
	.htotal = 1080 + 80 + 84 + 88,
	.vdisplay = 2400,
	.vsync_start = 2400 + 16,
	.vsync_end = 2400 + 16 + 2,
	.vtotal = 2400 + 16 + 2 + 2,
	.width_mm = 67,
	.height_mm = 150,
	.flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
};

static int s6e3fc3_panel_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode_120, *mode_60;

	mode_120 = drm_mode_duplicate(connector->dev, &ams646yd01_mode_120hz);
	if (!mode_120)
		return -ENOMEM;

	mode_120->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode_120);
	drm_mode_probed_add(connector, mode_120);

	mode_60 = drm_mode_duplicate(connector->dev, &ams646yd01_mode_60hz);
	if (mode_60) {
		mode_60->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_set_name(mode_60);
		drm_mode_probed_add(connector, mode_60);
	}

	connector->display_info.width_mm = ams646yd01_mode_120hz.width_mm;
	connector->display_info.height_mm = ams646yd01_mode_120hz.height_mm;

	return 2;
}

static const struct drm_panel_funcs s6e3fc3_panel_funcs = {
	.prepare = s6e3fc3_panel_prepare,
	.unprepare = s6e3fc3_panel_unprepare,
	.get_modes = s6e3fc3_panel_get_modes,
};

static int s6e3fc3_panel_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = (u16)backlight_get_brightness(bl);

	return mipi_dsi_dcs_set_display_brightness_large(dsi, brightness);
}

static const struct backlight_ops s6e3fc3_panel_bl_ops = {
	.update_status = s6e3fc3_panel_bl_update_status,
};

static struct backlight_device *s6e3fc3_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_PLATFORM,
		.brightness = 255,
		.max_brightness = 486,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &s6e3fc3_panel_bl_ops, &props);
}

static int s6e3fc3_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct s6e3fc3_panel *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->supply_vddio = devm_regulator_get_optional(dev, "vddio");
	if (IS_ERR(ctx->supply_vddio)) {
		if (PTR_ERR(ctx->supply_vddio) != -ENODEV)
			return PTR_ERR(ctx->supply_vddio);
		ctx->supply_vddio = NULL;
	}

	ctx->supply_vci = devm_regulator_get_optional(dev, "vci");
	if (IS_ERR(ctx->supply_vci)) {
		if (PTR_ERR(ctx->supply_vci) != -ENODEV)
			return PTR_ERR(ctx->supply_vci);
		ctx->supply_vci = NULL;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &s6e3fc3_panel_funcs, DRM_MODE_CONNECTOR_DSI);

	ctx->panel.backlight = s6e3fc3_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void s6e3fc3_panel_remove(struct mipi_dsi_device *dsi)
{
	struct s6e3fc3_panel *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id s6e3fc3_panel_of_match[] = {
	{ .compatible = "samsung,s6e3fc3-ams646yd01" },
	{ .compatible = "samsung,s6e3fc3-amb667bv01" },
	{ .compatible = "samsung,s6e3hab-amb667an01" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, s6e3fc3_panel_of_match);

static struct mipi_dsi_driver s6e3fc3_panel_driver = {
	.probe = s6e3fc3_panel_probe,
	.remove = s6e3fc3_panel_remove,
	.driver = {
		.name = "panel-samsung-s6e3fc3",
		.of_match_table = s6e3fc3_panel_of_match,
	},
};

module_mipi_dsi_driver(s6e3fc3_panel_driver);

MODULE_AUTHOR("Jiro <ktgithub28@gmail.com>");
MODULE_DESCRIPTION("DRM driver for Samsung AMOLED S6E3FC3 / S6E3HAB FHD+ panels");
MODULE_LICENSE("GPL");
