/*
 * QEMU GPIO backend
 *
 * Copyright 2020 Enrico Weigelt, metux IT consult <info@metux.net>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QEMU_GPIO_H
#define QEMU_GPIO_H

#include "qemu/queue.h"
#include "qom/object.h"

#define TYPE_GPIO_BACKEND "gpio-backend"
OBJECT_DECLARE_TYPE(GpioBackend, GpioBackendClass, GPIO_BACKEND)

#define TYPE_GPIO_BUILTIN "gpio-builtin"

/* dont change them - drivers rely on these values */
#define QEMU_GPIO_DIRECTION_OUTPUT  0
#define QEMU_GPIO_DIRECTION_INPUT   1

#define QEMU_GPIO_LINE_INACTIVE     0
#define QEMU_GPIO_LINE_ACTIVE       1

/*
 * notification callback from gpio backend to consumer/frontend
 *
 * consumer:    pointer to/for the consumer object
 * gpio_id:     id of the gpio (-1 = all at once)
 * event:       whats happened
 */
typedef int (*gpio_backend_notify_t)(void *consumer, int gpio, int event,
                                     int value);

#define GPIO_EVENT_INPUT  1
#define GPIO_EVENT_OUTPUT 2
#define GPIO_EVENT_LEVEL  3

struct GpioBackendClass {
    ObjectClass parent_class;
    bool opened;

    char *name;

    int (*request)(GpioBackend *s, int gpio);
    int (*direction_input)(GpioBackend *s, int gpio);
    int (*direction_output)(GpioBackend *s, int gpio, int state);
    int (*get_direction)(GpioBackend *s, int gpio);
    int (*set_value)(GpioBackend *s, int gpio, int state);
    int (*get_value)(GpioBackend *s, int gpio);
    int (*get_ngpio)(GpioBackend *s);
};

struct GpioBackend {
    Object parent;
    gpio_backend_notify_t notify_proc;
    void *notify_consumer;
};

/* call wrappers to gpio backend */
int gpio_backend_request(GpioBackend *s, int gpio);
int gpio_backend_direction_input(GpioBackend *s, int gpio);
int gpio_backend_direction_output(GpioBackend *s, int gpio, int state);
int gpio_backend_get_direction(GpioBackend *s, int gpio);
int gpio_backend_set_value(GpioBackend *s, int gpio, int state);
int gpio_backend_get_value(GpioBackend *s, int gpio);
int gpio_backend_set_notify(GpioBackend *s, gpio_backend_notify_t proc,
                            void *consumer);
int gpio_backend_send_notify(GpioBackend *s, int gpio, int event, int value);
int gpio_backend_get_ngpio(GpioBackend *s);

/* used by backend drivers for common initializations */
int gpio_backend_register(GpioBackend *s);
int gpio_backend_unregister(GpioBackend *s);

#endif /* QEMU_GPIO_H */
