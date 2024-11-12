/**
 * OpenALHelpers.h
 *
 * Copyright (C) Daniel Horn
 * Copyright (C) 2020 pyramid3d, Stephen G. Tuggy, and other Vega Strike
 * contributors
 * Copyright (C) 2022-2023 Stephen G. Tuggy, Benjamen R. Meyer
 *
 * https://github.com/vegastrike/Vega-Strike-Engine-Source
 *
 * This file is part of Vega Strike.
 *
 * Vega Strike is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Vega Strike is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Vega Strike.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef VEGA_STRIKE_ENGINE_AUDIO_RENDERERS_OPENAL_HELPERS_H
#define VEGA_STRIKE_ENGINE_AUDIO_RENDERERS_OPENAL_HELPERS_H

#include "../../Exceptions.h"
#include "al.h"

#define checkAlError() Audio::__impl::OpenAL::_checkAlErrorAt(alGetError(), __FILE__, __LINE__, true)
#define checkAlWarning() Audio::__impl::OpenAL::_checkAlErrorAt(alGetError(), __FILE__, __LINE__, false)
#define checkAlcError(x) Audio::__impl::OpenAL::_checkAlcErrorAt(x, __FILE__, __LINE__, true)
#define checkAlcWarning(x) Audio::__impl::OpenAL::_checkAlcErrorAt(x, __FILE__, __LINE__, false)
#define clearAlError() Audio::__impl::OpenAL::_clearAlError();

namespace Audio {
namespace __impl {
namespace OpenAL {

void _checkAlErrorAt(ALenum error, const char *filename, int lineno, bool errors_fatal);
void _checkAlcErrorAt(ALCdevice *device, const char *filename, int lineno, bool errors_fatal);
void _clearAlError();
ALenum asALFormat(const Format &format);

}
}
}

#endif //VEGA_STRIKE_ENGINE_AUDIO_RENDERERS_OPENAL_HELPERS_H
