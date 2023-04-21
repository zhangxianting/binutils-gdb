/* Manages interpreters for GDB, the GNU debugger.

   Copyright (C) 2000-2023 Free Software Foundation, Inc.

   Written by Jim Ingham <jingham@apple.com> of Apple Computer, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef INTERPS_H
#define INTERPS_H

#include "gdbsupport/intrusive_list.h"

struct bpstat;
struct ui_out;
struct interp;
struct ui;
class completion_tracker;
struct thread_info;
struct inferior;

typedef struct interp *(*interp_factory_func) (const char *name);

/* Each interpreter kind (CLI, MI, etc.) registers itself with a call
   to this function, passing along its name, and a pointer to a
   function that creates a new instance of an interpreter with that
   name.

   The memory for NAME must have static storage duration.  */
extern void interp_factory_register (const char *name,
				     interp_factory_func func);

extern void interp_exec (struct interp *interp, const char *command);

class interp : public intrusive_list_node<interp>
{
public:
  explicit interp (const char *name);
  virtual ~interp () = 0;

  virtual void init (bool top_level)
  {}

  virtual void resume () = 0;
  virtual void suspend () = 0;

  virtual void exec (const char *command) = 0;

  /* Returns the ui_out currently used to collect results for this
     interpreter.  It can be a formatter for stdout, as is the case
     for the console & mi outputs, or it might be a result
     formatter.  */
  virtual ui_out *interp_ui_out () = 0;

  /* Provides a hook for interpreters to do any additional
     setup/cleanup that they might need when logging is enabled or
     disabled.  */
  virtual void set_logging (ui_file_up logfile, bool logging_redirect,
			    bool debug_redirect) = 0;

  /* Called before starting an event loop, to give the interpreter a
     chance to e.g., print a prompt.  */
  virtual void pre_command_loop ()
  {}

  /* Returns true if this interpreter supports using the readline
     library; false if it uses GDB's own simplified readline
     emulation.  */
  virtual bool supports_command_editing ()
  { return false; }

  const char *name () const
  { return m_name; }

  /* Notify the interpreter that the current inferior has stopped with signal
     SIG.  */
  virtual void on_signal_received (gdb_signal sig) {}

  /* Notify the interpreter that the current inferior has exited with signal
     SIG. */
  virtual void on_signal_exited (gdb_signal sig) {}

  /* Notify the interpreter that the current inferior has stopped normally.  */
  virtual void on_normal_stop (bpstat *bs, int print_frame) {}

  /* Notify the intepreter that the current inferior has exited normally with
     status STATUS.  */
  virtual void on_exited (int status) {}

  /* Notify the interpreter that the current inferior has stopped reverse
     execution because there is no more history.  */
  virtual void on_no_history () {}

  /* Notify the interpreter that a synchronous command it started has
     finished.  */
  virtual void on_sync_execution_done () {}

  /* Notify the interpreter that an error was caught while executing a
     command on this interpreter.  */
  virtual void on_command_error () {}

  /* Notify the interpreter that the user focus has changed.  */
  virtual void on_user_selected_context_changed (user_selected_what selection)
    {}

  /* Notify the interpreter that thread T has been created.  */
  virtual void on_new_thread (thread_info *t) {}

  /* Notify the interpreter that thread T has exited.  */
  virtual void on_thread_exited (thread_info *, int silent) {}

  /* Notify the interpreter that inferior INF was added.  */
  virtual void on_inferior_added (inferior *inf) {}

  /* Notify the interpreter that inferior INF was started or attached.  */
  virtual void on_inferior_appeared (inferior *inf) {}

private:
  /* The memory for this is static, it comes from literal strings (e.g. "cli").  */
  const char *m_name;

public:
  /* Has the init method been run?  */
  bool inited = false;
};

/* Look up the interpreter for NAME, creating one if none exists yet.
   If NAME is not a interpreter type previously registered with
   interp_factory_register, return NULL; otherwise return a pointer to
   the interpreter.  */
extern struct interp *interp_lookup (struct ui *ui, const char *name);

/* Set the current UI's top level interpreter to the interpreter named
   NAME.  Throws an error if NAME is not a known interpreter or the
   interpreter fails to initialize.  */
extern void set_top_level_interpreter (const char *name);

/* Temporarily set the current interpreter, and reset it on
   destruction.  */
class scoped_restore_interp
{
public:

  scoped_restore_interp (const char *name)
    : m_interp (set_interp (name))
  {
  }

  ~scoped_restore_interp ()
  {
    set_interp (m_interp->name ());
  }

  scoped_restore_interp (const scoped_restore_interp &) = delete;
  scoped_restore_interp &operator= (const scoped_restore_interp &) = delete;

private:

  struct interp *set_interp (const char *name);

  struct interp *m_interp;
};

extern int current_interp_named_p (const char *name);

/* Call this function to give the current interpreter an opportunity
   to do any special handling of streams when logging is enabled or
   disabled.  LOGFILE is the stream for the log file when logging is
   starting and is NULL when logging is ending.  LOGGING_REDIRECT is
   the value of the "set logging redirect" setting.  If true, the
   interpreter should configure the output streams to send output only
   to the logfile.  If false, the interpreter should configure the
   output streams to send output to both the current output stream
   (i.e., the terminal) and the log file.  DEBUG_REDIRECT is same as
   LOGGING_REDIRECT, but for the value of "set logging debugredirect"
   instead.  */
extern void current_interp_set_logging (ui_file_up logfile,
					bool logging_redirect,
					bool debug_redirect);

/* Returns the top-level interpreter.  */
extern struct interp *top_level_interpreter (void);

/* Return the current UI's current interpreter.  */
extern struct interp *current_interpreter (void);

extern struct interp *command_interp (void);

extern void clear_interpreter_hooks (void);

/* Returns true if INTERP supports using the readline library; false
   if it uses GDB's own simplified form of readline.  */
extern int interp_supports_command_editing (struct interp *interp);

/* Called before starting an event loop, to give the interpreter a
   chance to e.g., print a prompt.  */
extern void interp_pre_command_loop (struct interp *interp);

/* List the possible interpreters which could complete the given
   text.  */
extern void interpreter_completer (struct cmd_list_element *ignore,
				   completion_tracker &tracker,
				   const char *text,
				   const char *word);

/* Notify all interpreters that the current inferior has stopped with signal
   SIG.  */
extern void interps_notify_signal_received (gdb_signal sig);

/* Notify all interpreters that the current inferior has exited with signal
   SIG.  */
extern void interps_notify_signal_exited (gdb_signal sig);

/* Notify all interpreters that the current inferior has stopped normally.  */
extern void interps_notify_normal_stop (bpstat *bs, int print_frame);

/* Notify all interpreters that the current inferior has stopped reverse
   execution because there is no more history.  */
extern void interps_notify_no_history ();

/* Notify all interpreters that the current inferior has exited normally with
   status STATUS.  */
extern void interps_notify_exited (int status);

/* Notify all interpreters that the user focus has changed.  */
extern void interps_notify_user_selected_context_changed
  (user_selected_what selection);

/* Notify all interpreters that thread T has been created.  */
extern void interps_notify_new_thread (thread_info *t);

/* Notify all interpreters that thread T has exited.  */
extern void interps_notify_thread_exited (thread_info *t, int silent);

/* Notify all interpreters that inferior INF was added.  */
extern void interps_notify_inferior_added (inferior *inf);

/* Notify all interpreters that inferior INF was started or attached.  */
extern void interps_notify_inferior_appeared (inferior *inf);

/* well-known interpreters */
#define INTERP_CONSOLE		"console"
#define INTERP_MI2             "mi2"
#define INTERP_MI3             "mi3"
#define INTERP_MI4             "mi4"
#define INTERP_MI		"mi"
#define INTERP_TUI		"tui"
#define INTERP_INSIGHT		"insight"

#endif
