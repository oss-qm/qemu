/*
 * QEMU GPIO Backend
 *
 * Copyright 2020 Enrico Weigelt, metux IT consult <info@metux.net>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <errno.h>
#include "qemu/osdep.h"
#include "sysemu/gpio.h"
#include "qapi/error.h"
#include "qapi/qmp/qerror.h"
#include "qemu/module.h"
#include "qom/object_interfaces.h"
#include "qemu/error-report.h"
#include "sysemu/gpio.h"

#define GPIO_FORMAT_VALUE       "gpio%d.value"
#define GPIO_FORMAT_DIRECTION   "gpio%d.direction"

#define BACKEND_OP_HEAD \
    if (!gpio) \
        return -EFAULT;

#define BACKEND_CLASSOP_HEAD(op) \
    GpioBackendClass *klass; \
    BACKEND_OP_HEAD \
    klass = GPIO_BACKEND_GET_CLASS(gpio); \
    if (!klass) \
        return -EFAULT; \
    if (!klass->op) \
        return -EUNATCH;

int gpio_backend_set_notify(GpioBackend *gpio,
                            gpio_backend_notify_t proc,
                            void *consumer)
{
    BACKEND_OP_HEAD

    gpio->notify_proc = proc;
    gpio->notify_consumer = consumer;

    return 0;
}

int gpio_backend_send_notify(GpioBackend *gpio, int pin, int event, int value)
{
    BACKEND_OP_HEAD

    if (gpio->notify_proc) {
        return gpio->notify_proc(gpio->notify_consumer, pin, event, value);
    }

    return 0;
}

int gpio_backend_request(GpioBackend *gpio, int pin)
{
    BACKEND_CLASSOP_HEAD(request);
    return klass->request(gpio, pin);
}

int gpio_backend_set_value(GpioBackend *gpio, int pin, int state)
{
    BACKEND_CLASSOP_HEAD(set_value);
    return klass->set_value(gpio, pin, state);
}

int gpio_backend_get_value(GpioBackend *gpio, int pin)
{
    BACKEND_CLASSOP_HEAD(get_value);
    return klass->get_value(gpio, pin);
}

int gpio_backend_direction_output(GpioBackend *gpio, int pin, int state)
{
    BACKEND_CLASSOP_HEAD(direction_output);
    return klass->direction_output(gpio, pin, state);
}

int gpio_backend_direction_input(GpioBackend *gpio, int pin)
{
    BACKEND_CLASSOP_HEAD(direction_input);
    return klass->direction_input(gpio, pin);
}

int gpio_backend_get_direction(GpioBackend *gpio, int pin)
{
    BACKEND_CLASSOP_HEAD(get_direction);
    return klass->get_direction(gpio, pin);
}

int gpio_backend_get_ngpio(GpioBackend *gpio)
{
    BACKEND_CLASSOP_HEAD(get_ngpio);
    return klass->get_ngpio(gpio);
}

static void getattr_value(Object *obj, Visitor *v, const char *name,
                          void *opaque, Error **errp)
{
    int pin;
    int64_t val = 0;
    GpioBackend *gpio = GPIO_BACKEND(obj);

    if (sscanf(name, GPIO_FORMAT_VALUE, &pin) != 1) {
        error_setg(errp,
                  "gpio: getattr_value() illegal property: \"%s\"",
                   name);
        return;
    }

    val = gpio_backend_get_value(gpio, pin);
    visit_type_int(v, name, &val, errp);
}

static void setattr_value(Object *obj, Visitor *v, const char *name,
                          void *opaque, Error **errp)
{
    int pin;
    int64_t val = 0;
    GpioBackend *gpio = GPIO_BACKEND(obj);

    if (!visit_type_int(v, name, &val, errp)) {
        return;
    }

    if (sscanf(name, GPIO_FORMAT_VALUE, &pin) != 1) {
        error_setg(errp,
                   "gpio: setattr_value() illegal property: \"%s\"",
                   name);
        return;
    }

    gpio_backend_set_value(gpio, pin, val);
    gpio_backend_send_notify(gpio, pin, GPIO_EVENT_LEVEL, val);
}

static void getattr_direction(Object *obj, Visitor *v, const char *name,
                              void *opaque, Error **errp)
{
    int pin;
    GpioBackend *gpio = GPIO_BACKEND(obj);
    char str[16] = { 0 };
    char *val = str;

    if (sscanf(name, GPIO_FORMAT_DIRECTION, &pin) != 1) {
        error_setg(errp,
                   "gpio: getattr_direction() illegal property: \"%s\"",
                   name);
        return;
    }

    strcpy(str, (gpio_backend_get_direction(gpio, pin)
                    == QEMU_GPIO_DIRECTION_INPUT) ? "in" : "out");
    visit_type_str(v, name, &val, errp);
}

static void setattr_direction(Object *obj, Visitor *v, const char *name,
                              void *opaque, Error **errp)
{
    int pin;
    GpioBackend *gpio = GPIO_BACKEND(obj);
    char *val;

    if (!visit_type_str(v, name, &val, errp)) {
        return;
    }

    if (sscanf(name, GPIO_FORMAT_DIRECTION, &pin) != 1) {
        error_setg(errp, "gpio: setattr_direction() illegal property: \"%s\"",
                   name);
        return;
    }

    if (strcmp(val, "in") == 0) {
        gpio_backend_direction_input(gpio, pin);
        gpio_backend_send_notify(gpio, pin, GPIO_EVENT_INPUT, 0);
        return;
    }

    if (strcmp(val, "out") == 0) {
        gpio_backend_direction_output(gpio, pin, QEMU_GPIO_LINE_INACTIVE);
        gpio_backend_send_notify(gpio, pin, GPIO_EVENT_OUTPUT, 0);
        return;
    }

    error_setg(errp, "gpio: setattr_direction() illegal value: \"%s\"", val);
    return;
}

int gpio_backend_register(GpioBackend *gpio)
{
    int pin;
    int ngpio = gpio_backend_get_ngpio(gpio);

    if (ngpio < 1) {
        error_report("gpio_backend_register() illegal number of gpios: %d",
                     ngpio);
        return -EINVAL;
    }

    for (pin = 0; pin < ngpio; pin++) {
        char name[64];
        snprintf(name, sizeof(name), GPIO_FORMAT_VALUE, pin);
        object_property_add(OBJECT(gpio), name, "bool", getattr_value,
                            setattr_value, NULL, NULL);
        snprintf(name, sizeof(name), GPIO_FORMAT_DIRECTION, pin);
        object_property_add(OBJECT(gpio), name, "string", getattr_direction,
                            setattr_direction, NULL, NULL);
    }

    return 0;
}

int gpio_backend_unregister(GpioBackend *s)
{
    /* nothing to do for now */
    return 0;
}

static void gpio_backend_init(Object *obj)
{
}

static void gpio_backend_finalize(Object *obj)
{
}

static void gpio_backend_class_init(ObjectClass *oc, void *data)
{
}

static const TypeInfo gpio_backend_info = {
    .name = TYPE_GPIO_BACKEND,
    .parent = TYPE_OBJECT,
    .instance_size = sizeof(GpioBackend),
    .instance_init = gpio_backend_init,
    .instance_finalize = gpio_backend_finalize,
    .class_size = sizeof(GpioBackendClass),
    .class_init = gpio_backend_class_init,
    .abstract = true,
    .interfaces = (InterfaceInfo[]) {
        { TYPE_USER_CREATABLE },
        { }
    }
};

static void register_types(void)
{
    type_register_static(&gpio_backend_info);
}

type_init(register_types);
