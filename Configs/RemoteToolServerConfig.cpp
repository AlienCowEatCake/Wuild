/*
 * Copyright (C) 2017 Smirnov Vladimir mapron1@gmail.com
 * Source code licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0 or in file COPYING-APACHE-2.0.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.h
 */

#include "RemoteToolServerConfig.h"

#include <iostream>

namespace Wuild
{

bool RemoteToolServerConfig::Validate(std::ostream *errStream) const
{
    if (m_listenHost.empty())
    {
        if (errStream)
            *errStream << "Invalid listenHost.";
        return false;
    }
    if (m_listenPort <= 0)
    {
        if (errStream)
            *errStream << "Invalid listenPort.";
        return false;
    }
    if (m_workersCount <= 0)
    {
        if (errStream)
            *errStream << "workersCount: Number of thread should be greater than zero.";
        return false;
    }

    return m_coordinator.Validate(errStream);
}

}