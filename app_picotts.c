/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2016 Jerry Murdock <gitjerrm@outlook.com>
 * Copyright (C) 2012 Ramon Martinez <rampa@encomix.org>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the COPYING file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief Say text to the user, using PicoTTS TTS engine.
 *
 * \author\verbatim Jerry Murdock based on works of Ramon Martinez <rampa@encomix.org> and Lefteris Zafiris <zaf.000@gmail.com>\endverbatim
 *
 * \extref PicoTTS text to speech Synthesis System
 *
 * \ingroup applications
 */

/*** MODULEINFO
        <defaultenabled>no</defaultenabled>
 ***/

#include "asterisk.h"

#include <stdio.h>
#include <string.h>
#include <utime.h>

#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/module.h"
#include "asterisk/config.h"
#include "asterisk/app.h"
#include "asterisk/utils.h"
#include "asterisk/strings.h"

//  return utime(fn, NULL);

#define AST_MODULE "PicoTTS"

#define PICO_CONFIG "app_picotts.conf"
#define MAXLEN 2048
#define DEF_RATE 8000
#define DEF_LANG "en-US"
#define DEF_DIR "/tmp"
#define DEF_VOLUME 0.8


static char *app = AST_MODULE;
static char *synopsis = "Say text to the user, using PicoTTS TTS engine";
static char *descrip =
  " PicoTTS(text[,intkeys][,language]): This will invoke the PicoTTS TTS engine, send a text string,\n"
  "get back the resulting waveform and play it to the user, allowing any given interrupt\n"
  "keys to immediately terminate and return the value, or 'any' to allow any number back.\n";

static int target_sample_rate;
static int usecache;
static int touchcache;
static double volume;
static const char *cachedir;
static const char *voice_lang;
static const char *effects;
static struct ast_config *cfg;
static struct ast_flags config_flags = { 0 };

static int read_config(const char *pico_conf)
{
  const char *temp;

  /* set default values */
  target_sample_rate = DEF_RATE;
  usecache = 0;
  cachedir = DEF_DIR;
  voice_lang = DEF_LANG;
  volume = DEF_VOLUME;
  effects = "";

  cfg = ast_config_load(pico_conf, config_flags);
  if (!cfg || cfg == CONFIG_STATUS_FILEINVALID)
    {
      ast_log(LOG_WARNING,
              "PicoTTS: Unable to read config file %s. Using default settings\n",
              PICO_CONFIG);
    }
  else
    {
      if ((temp = ast_variable_retrieve(cfg, "general", "usecache")))
        usecache = ast_true(temp);

      if ((temp = ast_variable_retrieve(cfg, "general", "cachedir")))
        cachedir = temp;

      if ((temp = ast_variable_retrieve(cfg, "general", "touchcache")))
        touchcache = ast_true(temp);

      if ((temp = ast_variable_retrieve(cfg, "general", "language")))
        voice_lang = temp;

      if ((temp = ast_variable_retrieve(cfg, "general", "soxeffects")))
        effects = temp;

      if ((temp = ast_variable_retrieve(cfg, "general", "samplerate")))
        target_sample_rate = atoi(temp);

      if ((temp = ast_variable_retrieve(cfg, "general", "volume")))
        volume = atof(temp);
    }


  if (target_sample_rate != 8000 && target_sample_rate != 16000)
    {
      ast_log(LOG_WARNING, "PicoTTS: Unsupported sample rate: %d. Falling back to %d\n",
              target_sample_rate, DEF_RATE);
      target_sample_rate = DEF_RATE;
    }
  return 0;
}


static int picotts_text_to_wave(const char *filedata, const char *language, const char *texttospeech)
{
  int res_tts = 0;
  char temp[2048];

  snprintf(temp, sizeof(temp), "pico2wave -w %s -l %s '%s'", filedata, language, texttospeech);

  res_tts = system(temp);
  ast_log(LOG_WARNING, "PicoTTS: command %s, code %d.\n", temp, res_tts);

  return(0);
}


static int picotts_exec(struct ast_channel *chan, const char *data)
{
  int res = 0;
  char *mydata;
  int writecache = 0;
  char MD5_name[33] = "";
  char cachefile[MAXLEN] = "";
  char touchfile[MAXLEN] = "";
  char tmp_name[200];
  char raw_tmp_name[200];
  char rawpico_tmp_name[200];
  char ext[6] = "";

//  read_config(PICO_CONFIG);

  AST_DECLARE_APP_ARGS(args,
                       AST_APP_ARG(text);
                       AST_APP_ARG(interrupt);
                       AST_APP_ARG(lang);
                       );

  if (ast_strlen_zero(data))
    {
      ast_log(LOG_ERROR, "PicoTTS requires an argument (text)\n");
      return -1;
    }

  mydata = ast_strdupa(data);
  AST_STANDARD_APP_ARGS(args, mydata);


  if (args.interrupt && !strcasecmp(args.interrupt, "any"))
    args.interrupt = AST_DIGIT_ANY;


  args.text = ast_strip_quoted(args.text, "\"", "\"");
  if (ast_strlen_zero(args.text)) {
      ast_log(LOG_WARNING, "PicoTTS: No text passed for synthesis.\n");
      return res;
    }

  if (args.lang)
      ast_strip_quoted(args.lang, "\"", "\"");

  if (ast_strlen_zero(args.lang))   {
      ast_log(LOG_WARNING, "PicoTTS: language is default: %s.\n", voice_lang);
    }
  else
    {
      voice_lang = args.lang;
    }

  ast_debug(1, "PicoTTS:\nText passed: %s\nInterrupt key(s): %s\nVoice: %s\nRate: %d\n",
            args.text, args.interrupt, voice_lang, target_sample_rate);


  /* Create filenames */
  if (target_sample_rate == 16000)
      snprintf(ext, sizeof(ext), "sln16");
  else
      snprintf(ext, sizeof(ext), "sln");

  snprintf(tmp_name, sizeof(tmp_name), "/tmp/picotts_%li", ast_random() % 99999999);
  snprintf(rawpico_tmp_name, sizeof(rawpico_tmp_name), "%s.wav", tmp_name);
  snprintf(raw_tmp_name, sizeof(raw_tmp_name), "%s.%s", tmp_name, ext);

  /*Cache mechanism */
  if (usecache)
    {
      ast_md5_hash(MD5_name, args.text);
      if (strlen(cachedir) + strlen(MD5_name) + 8 <= MAXLEN)
        {
          ast_debug(1, "PicoTTS: Activating cache mechanism...\n");
          snprintf(cachefile, sizeof(cachefile), "%s/%s", cachedir, MD5_name);
          if (ast_fileexists(cachefile, ext, NULL) <= 0)
            {
              ast_debug(1, "PicoTTS: Cache file does not yet exist.\n");
              writecache = 1;
            }
          else
            {
              ast_debug(1, "PicoTTS: Cache file exists.\n");
              if (ast_channel_state(chan) != AST_STATE_UP)
                ast_answer(chan);
              res = ast_streamfile(chan, cachefile, ast_channel_language(chan));
              if (res)
                {
                  ast_log(LOG_ERROR, "PicoTTS: ast_streamfile from cache failed on %s\n",
                           ast_channel_name(chan));
                }
              else
                {
                  res = ast_waitstream(chan, args.interrupt);
                  ast_stopstream(chan);
                  if (touchcache)
                    {
                      snprintf(touchfile, sizeof(touchfile), "%s.%s", cachefile, ext);
                      if (utime(touchfile, NULL) != 0)
                        ast_log(LOG_ERROR, "PicoTTS: could not update timestamp on  %s\n",
                                 touchfile);
                    }
                  return res;
                }
            }
        }
    }


/* Invoke PicoTTS */

  if (strcmp(voice_lang, "en-US") == 0)
    ;
  else if (strcmp(voice_lang, "en-GB") == 0)
    ;
  else if (strcmp(voice_lang, "de-DE") == 0)
    ;
  else if (strcmp(voice_lang, "es-ES") == 0)
    ;
  else if (strcmp(voice_lang, "fr-FR") == 0)
    ;
  else if (strcmp(voice_lang, "it-IT") == 0)
    ;
  else
    {
      ast_log(LOG_WARNING, "PicoTTS: Unsupported voice %s. Using default voice.\n",
              voice_lang);
      voice_lang = DEF_LANG;
    }

  res = picotts_text_to_wave(rawpico_tmp_name, voice_lang, args.text);

  char temp[1024];
  snprintf(temp, sizeof(temp), "sox -v %f  %s -q -r %d -c1 -t raw %s %s", volume, rawpico_tmp_name, target_sample_rate, raw_tmp_name, effects);
  res = system(temp);
  ast_log(LOG_WARNING, "PicoTTS: command %s, code %d.\n", temp, res);
  unlink(rawpico_tmp_name);

  if (writecache)
    {
      ast_debug(1, "PicoTTS: Saving cache file %s\n", cachefile);
      ast_filecopy(tmp_name, cachefile, NULL);
    }

  if (ast_channel_state(chan) != AST_STATE_UP)
    ast_answer(chan);
  res = ast_streamfile(chan, tmp_name, ast_channel_language(chan));
  if (res)
    {
      ast_log(LOG_ERROR, "PicoTTS: ast_streamfile failed on %s\n", ast_channel_name(chan));
    }
  else
    {
      res = ast_waitstream(chan, args.interrupt);
      ast_stopstream(chan);
    }

  ast_filedelete(tmp_name, NULL);
  return res;
}

static int reload_module(void)
{
  ast_config_destroy(cfg);
  read_config(PICO_CONFIG);
  return 0;
}

static int unload_module(void)
{
  ast_config_destroy(cfg);
  return ast_unregister_application(app);
}

static int load_module(void)
{
  read_config(PICO_CONFIG);
  return ast_register_application(app, picotts_exec, synopsis, descrip) ?
         AST_MODULE_LOAD_DECLINE : AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "PicoTTS TTS Interface",
                .load = load_module,
                .unload = unload_module,
                .reload = reload_module,
                );
