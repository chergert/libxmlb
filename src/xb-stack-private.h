/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-opcode-private.h"
#include "xb-stack.h"

G_BEGIN_DECLS

/* Members of this struct should not be accessed directly — use the accessor
 * functions below instead. */
struct _XbStack {
	/*< private >*/
	gint ref;
	gboolean stack_allocated; /* whether this XbStack was allocated with alloca() */
	guint pos;		  /* index of the next unused entry in .opcodes */
	guint max_size;
	XbOpcode opcodes[]; /* allocated as part of XbStack */
};

/**
 * xb_stack_new_inline:
 * @max_stack_size: maximum size of the stack
 *
 * Creates a stack for the XbMachine request. Only #XbOpcodes can be pushed and
 * popped from the stack.
 *
 * The stack will be allocated on the current C stack frame, so @max_stack_size
 * should be chosen to not overflow the C process’ stack.
 *
 * Returns: (transfer full): a #XbStack
 *
 * Since: 0.3.1
 **/
#define xb_stack_new_inline(max_stack_size)                                                        \
	(G_GNUC_EXTENSION({                                                                        \
		/* This function has to be static inline so we can use g_alloca(), which           \
		 * is needed for performance reasons — about 3 million XbStacks are              \
		 * allocated while starting gnome-software. */                                     \
		guint xsni_max_size = (max_stack_size);                                            \
		XbStack *xsni_stack =                                                              \
		    g_alloca(sizeof(XbStack) + xsni_max_size * sizeof(XbOpcode));                  \
		xsni_stack->ref = 1;                                                               \
		xsni_stack->stack_allocated = TRUE;                                                \
		xsni_stack->pos = 0;                                                               \
		xsni_stack->max_size = xsni_max_size;                                              \
		(XbStack *)xsni_stack;                                                             \
	}))

XbStack *
xb_stack_new(guint max_size);
void
xb_stack_unref(XbStack *self);
XbStack *
xb_stack_ref(XbStack *self);
guint
xb_stack_get_size(XbStack *self);
guint
xb_stack_get_max_size(XbStack *self);
gboolean
xb_stack_pop_two(XbStack *self, XbOpcode *opcode1_out, XbOpcode *opcode2_out, GError **error);
gboolean
xb_stack_push_bool(XbStack *self, gboolean val, GError **error);
XbOpcode *
xb_stack_peek(XbStack *self, guint idx);
XbOpcode *
xb_stack_peek_head(XbStack *self);
XbOpcode *
xb_stack_peek_tail(XbStack *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XbStack, xb_stack_unref)

static inline XbOpcode *
_xb_stack_peek(const XbStack *self, guint idx)
{
	if (idx >= self->pos)
		return NULL;
	return &self->opcodes[idx];
}

static inline gboolean
_xb_stack_pop(XbStack *self, XbOpcode *opcode_out, GError **error)
{
	if G_UNLIKELY(self->pos == 0) {
		if (error != NULL)
			g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "stack is empty");
		return FALSE;
	}
	self->pos--;
	if (opcode_out != NULL)
		*opcode_out = self->opcodes[self->pos];
	return TRUE;
}

static inline gboolean
_xb_stack_pop_two(XbStack *self, XbOpcode *opcode1_out, XbOpcode *opcode2_out, GError **error)
{
	if G_UNLIKELY(self->pos < 2) {
		if (error != NULL)
			g_set_error_literal(error,
					    G_IO_ERROR,
					    G_IO_ERROR_INVALID_DATA,
					    "stack is not full enough");
		return FALSE;
	}
	if (opcode1_out != NULL)
		*opcode1_out = self->opcodes[self->pos - 1];
	if (opcode2_out != NULL)
		*opcode2_out = self->opcodes[self->pos - 2];
	self->pos -= 2;
	return TRUE;
}

static inline XbOpcode *
_xb_stack_peek_head(const XbStack *self)
{
	if (self->pos == 0)
		return NULL;
	return &self->opcodes[0];
}

static inline XbOpcode *
_xb_stack_peek_tail(const XbStack *self)
{
	if (self->pos == 0)
		return NULL;
	return &self->opcodes[self->pos - 1];
}

static inline gboolean
_xb_stack_push(XbStack *self, XbOpcode **opcode_out, GError **error)
{
	if G_UNLIKELY (self->pos >= self->max_size) {
		*opcode_out = NULL;
		if (error != NULL)
			g_set_error(error,
				    G_IO_ERROR,
				    G_IO_ERROR_NO_SPACE,
				    "stack is already at maximum size of %u",
				    self->max_size);
		return FALSE;
	}

	*opcode_out = &self->opcodes[self->pos++];
	return TRUE;
}

static inline gboolean
_xb_stack_push_bool(XbStack *self, gboolean val, GError **error)
{
	XbOpcode *op;
	if (!_xb_stack_push(self, &op, error))
		return FALSE;
	xb_opcode_bool_init(op, val);
	return TRUE;
}

/* Push two opcodes onto the stack with appropriate rollback on failure. */
static inline gboolean
_xb_stack_push_two(XbStack *opcodes, XbOpcode **op1, XbOpcode **op2, GError **error)
{
	if (!_xb_stack_push(opcodes, op1, error))
		return FALSE;
	if (!_xb_stack_push(opcodes, op2, error)) {
		_xb_stack_pop(opcodes, NULL, NULL);
		return FALSE;
	}
	return TRUE;
}

static inline guint
_xb_stack_get_size(XbStack *self)
{
	return self->pos;
}

G_END_DECLS
