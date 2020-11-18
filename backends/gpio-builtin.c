/*
 * QEMU GPIO Backend - builtin (dummy)
 *
 * Copyright 2020 Enrico Weigelt, metux IT consult <info@metux.net>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "sysemu/gpio.h"
#include "qemu/main-loop.h"
#include "qemu/guest-random.h"
#include "qom/object.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "qapi/visitor.h"

#define MAX_GPIO                256

#define WARN(...)               warn_report("gpio-builtin: " __VA_ARGS__)

#define OP_HEAD(name) \
    GpioBuiltin *gpio = GPIO_BUILTIN(obj); \
    if (id >= gpio->num_gpio) { \
        WARN("%s: gpio id %d out of range", name, id); \
        return -ERANGE; \
    }

#define FLAG_DIRECTION_INPUT    1
#define FLAG_LINE_ACTIVE        2

OBJECT_DECLARE_SIMPLE_TYPE(GpioBuiltin, GPIO_BUILTIN)

struct GpioBuiltin {
    GpioBackend parent;
    char *states;
    int num_gpio;
};

static int gpio_builtin_request(GpioBackend *obj, int id)
{
    OP_HEAD("request");
    return 0;
}

static int gpio_builtin_set_value(GpioBackend *obj, int id, int state)
{
    OP_HEAD("set");
    if (state & QEMU_GPIO_LINE_ACTIVE) {
        gpio->states[id] |= FLAG_LINE_ACTIVE;
    } else {
        gpio->states[id] &= ~FLAG_LINE_ACTIVE;
    }
    return 0;
}

static int gpio_builtin_direction_input(GpioBackend *obj, int id)
{
    OP_HEAD("direction-input");
    gpio->states[id] |= FLAG_DIRECTION_INPUT;
    return gpio_builtin_set_value(obj, id, 0);
}

static int gpio_builtin_direction_output(GpioBackend *obj, int id, int state)
{
    OP_HEAD("direction-output");
    gpio->states[id] &= ~FLAG_DIRECTION_INPUT;
    return gpio_builtin_set_value(obj, id, state);
}

static int gpio_builtin_get_direction(GpioBackend *obj, int id)
{
    OP_HEAD("get-direction");
    return (gpio->states[id] & FLAG_DIRECTION_INPUT ?
            QEMU_GPIO_DIRECTION_INPUT : QEMU_GPIO_DIRECTION_OUTPUT);
}

static int gpio_builtin_get_value(GpioBackend *obj, int id)
{
    OP_HEAD("get");
    return (gpio->states[id] & FLAG_LINE_ACTIVE ?
            QEMU_GPIO_LINE_ACTIVE : QEMU_GPIO_LINE_INACTIVE);
}

static void gpio_builtin_instance_init(Object *obj)
{
    GpioBuiltin *gpio = GPIO_BUILTIN(obj);

    gpio->num_gpio = MAX_GPIO;
    gpio->states = g_malloc(gpio->num_gpio + 1);
    memset(gpio->states, 'i', gpio->num_gpio);
    gpio->states[gpio->num_gpio] = 0;
    gpio_backend_register(&gpio->parent);
}

static void gpio_builtin_instance_finalize(Object *obj)
{
    GpioBuiltin *gpio = GPIO_BUILTIN(obj);
    gpio_backend_unregister(&gpio->parent);
    g_free(gpio->states);
}

static int gpio_builtin_get_ngpio(GpioBackend *obj)
{
    GpioBuiltin *gpio = GPIO_BUILTIN(obj);
    return gpio->num_gpio;
}

static void gpio_builtin_class_init(ObjectClass *klass, void *data)
{
    GpioBackendClass *gpio = GPIO_BACKEND_CLASS(klass);

    gpio->name             = g_strdup("gpio-builtin");
    gpio->get_value        = gpio_builtin_get_value;
    gpio->set_value        = gpio_builtin_set_value;
    gpio->get_direction    = gpio_builtin_get_direction;
    gpio->direction_input  = gpio_builtin_direction_input;
    gpio->direction_output = gpio_builtin_direction_output;
    gpio->request          = gpio_builtin_request;
    gpio->get_ngpio        = gpio_builtin_get_ngpio;
}

static const TypeInfo gpio_builtin_info = {
    .name = TYPE_GPIO_BUILTIN,
    .parent = TYPE_GPIO_BACKEND,
    .instance_size = sizeof(GpioBuiltin),
    .instance_init = gpio_builtin_instance_init,
    .instance_finalize = gpio_builtin_instance_finalize,
    .class_init = gpio_builtin_class_init,
};

static void register_types(void)
{
    type_register_static(&gpio_builtin_info);
}

type_init(register_types);
