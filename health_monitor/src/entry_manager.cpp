/*
 * Copyright (C) 2019 LEIDOS.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#include "entry_manager.h"

namespace health_monitor
{

    EntryManager::EntryManager() {}
    
    EntryManager::EntryManager(const std::vector<std::string> required_entries)
    {
        this->required_entries_ = required_entries;
    }

    void EntryManager::update_entry(Entry entry)
    {
        for(auto i = entry_list_.begin(); i < entry_list_.end(); ++i)
        {
            if(i->name_.compare(entry.name_) == 0)
            {
                // name and type of the entry wont change
                i->active_ = entry.active_;
                i->available_ = entry.available_;
                i->timestamp_ = entry.timestamp_;
                return;
            }
        }
        entry_list_.push_back(entry);
    }

    std::vector<Entry> EntryManager::get_entries() const
    {
        // returns the copy of the original list
        return std::vector<Entry>(entry_list_);
    }

    void EntryManager::delete_entry(std::string name)
    {
        for(auto i = entry_list_.begin(); i < entry_list_.end(); ++i)
        {
            if(i->name_.compare(name) == 0)
            {
                entry_list_.erase(i);
                return;
            }
        }
    }

    boost::optional<Entry> EntryManager::get_entry_by_name(std::string name) const
    {
        for(auto i = entry_list_.begin(); i < entry_list_.end(); ++i)
        {
            if(i->name_.compare(name) == 0)
            {
                return *i;
            }
        }
        // use boost::optional because requested entry might not exist
        return boost::optional<Entry>();
    }

    bool EntryManager::is_entry_required(const std::string name) const
    {
        for(auto i = required_entries_.begin(); i < required_entries_.end(); ++i)
        {
            if(i->compare(name) == 0)
            {
                return true;
            }
        }
        return false;
    }

}