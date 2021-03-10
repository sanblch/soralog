/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soralog/logger_system.hpp>

#include <set>

#include <soralog/group.hpp>
#include <soralog/logger.hpp>

namespace soralog {

  void LoggerSystem::makeGroup(std::string name,
                               const std::optional<std::string> &parent,
                               const std::optional<std::string> &sink,
                               const std::optional<Level> &level) {
    auto group =
        std::make_shared<Group>(*this, std::move(name), parent, sink, level);
    if (not fallback_group_) {
      fallback_group_ = group;
      groups_["*"] = group;
    }
    groups_[group->name()] = std::move(group);
  }

  Configurator::Result LoggerSystem::configure() {
    if (is_configured_) {
      throw std::logic_error("LoggerSystem is already configured");
    }
    is_configured_ = true;
    auto result = configurator_->applyOn(*this);

    if (groups_.empty()) {
      result.message +=
          "E: No one group is defined; "
          "Logging system is unworkable\n";
      result.has_error = true;
    }
    for (auto &item : groups_) {
      if (item.first == "*") {
        continue;
      }
      if (item.second->sink()->name() == "*") {
        result.message +=
            "W: Group '" + item.first + "' has undefined sink;"
            "Sink to nowhere will be used\n";
        result.has_warning = true;
      }
    }

    return result;
  }

  std::shared_ptr<Logger> LoggerSystem::getLogger(
      std::string logger_name, const std::string &group_name,
      const std::optional<std::string> &sink_name,
      const std::optional<Level> &level) {
    if (not is_configured_) {
      throw std::logic_error("LoggerSystem is not yet configured");
    }

    if (auto it = loggers_.find(logger_name); it != loggers_.end()) {
      if (auto logger = it->second.lock()) {
        return std::move(logger);
      }
    }

    auto group = getGroup(group_name);
    if (group == nullptr) {
      group = fallback_group_;
      assert(group != nullptr);  // At least fallback group must be existing
      auto logger = std::make_shared<Logger>(*this, "Soralog", group);
      logger->warn(
          "Group '{}' for logger '{}' is not found. "
          "Fallback group will be used. Use '*' group for using default one",
          group_name, logger_name);
    }

    auto logger = std::make_shared<Logger>(*this, std::move(logger_name),
                                           std::move(group));

    if (sink_name.has_value()) {
      logger->setSink(sink_name.value());
    }

    if (level.has_value()) {
      logger->setLevel(level.value());
    }

    loggers_[logger->name()] = logger;
    return logger;
  }

  [[nodiscard]] std::shared_ptr<Sink> LoggerSystem::getSink(
      const std::string &sink_name) {
    auto it = sinks_.find(sink_name);
    if (it == sinks_.end()) {
      return {};
    }
    return it->second;
  }

  [[nodiscard]] std::shared_ptr<Group> LoggerSystem::getGroup(
      const std::string &group_name) {
    auto it = groups_.find(group_name);
    if (it == groups_.end()) {
      return {};
    }
    return it->second;
  }

  void LoggerSystem::setParentOfGroup(const std::shared_ptr<Group> &group,
                                      const std::shared_ptr<Group> &parent) {
    assert(group != nullptr);
    assert(parent != nullptr);

    group->setParentGroup(parent);

    std::map<std::shared_ptr<const Group>, int> passed_groups;

    std::vector<std::set<std::shared_ptr<Group>>> affecting_groups;

    std::function<size_t(const std::shared_ptr<const Group> &)> fn =
        [&](const std::shared_ptr<const Group> &current) mutable -> size_t {
      if (auto it = passed_groups.find(current); it != passed_groups.end()) {
        return it->second;
      }
      if (current->isLevelOverriden() && current->isSinkOverriden()) {
        return -1;
      }
      if (current == group) {
        return 0;
      }
      if (not current->parent()) {
        return -1;
      }
      auto n = fn(current->parent());
      if (n == -1) {
        return -1;
      }
      affecting_groups[n].emplace(std::const_pointer_cast<Group>(current));
      return ++n;
    };

    for (const auto &it : groups_) {
      auto n = fn(it.second);
      passed_groups[it.second] = n;
    }

    for (const auto &stage : std::move(affecting_groups)) {
      for (const auto &changing_group : stage) {
        changing_group->setParentGroup(changing_group->parent());
      }
    }

    for (auto it = loggers_.begin(); it != loggers_.end();) {
      if (auto logger = it->second.lock()) {
        if (auto it2 = passed_groups.find(logger->group());
            it2 != passed_groups.end()) {
          if (it2->second != -1) {
            logger->setGroup(logger->group());
          }
        }
        ++it;
      } else {
        it = loggers_.erase(it);
      }
    }
  }

  void LoggerSystem::setSinkOfGroup(const std::shared_ptr<Group> &group,
                                    std::optional<std::shared_ptr<Sink>> sink) {
    assert(group != nullptr);

    if (sink) {
      group->setSink(*sink);
    } else {
      group->resetSink();
    }

    std::map<std::shared_ptr<const Group>, int> passed_groups;

    std::vector<std::set<std::shared_ptr<Group>>> affecting_groups;

    std::function<size_t(const std::shared_ptr<const Group> &)> fn =
        [&](const std::shared_ptr<const Group> &current) mutable -> size_t {
      if (auto it = passed_groups.find(current); it != passed_groups.end()) {
        return it->second;
      }
      if (current->isSinkOverriden()) {
        return -1;
      }
      if (current == group) {
        return 0;
      }
      if (not current->parent()) {
        return -1;
      }
      auto n = fn(current->parent());
      if (n == -1) {
        return -1;
      }
      affecting_groups[n].emplace(std::const_pointer_cast<Group>(current));
      return ++n;
    };

    for (const auto &it : groups_) {
      auto n = fn(it.second);
      passed_groups[it.second] = n;
    }

    for (const auto &stage : std::move(affecting_groups)) {
      for (const auto &changing_group : stage) {
        changing_group->setSinkFromGroup(changing_group->parent());
      }
    }

    for (auto it = loggers_.begin(); it != loggers_.end();) {
      if (auto logger = it->second.lock()) {
        if (auto it2 = passed_groups.find(logger->group());
            it2 != passed_groups.end()) {
          if (it2->second != -1) {
            logger->setGroup(logger->group());
          }
        }
        ++it;
      } else {
        it = loggers_.erase(it);
      }
    }
  }

  void LoggerSystem::setLevelOfGroup(const std::shared_ptr<Group> &group,
                                     std::optional<Level> level) {
    assert(group != nullptr);

    if (level) {
      group->setLevel(*level);
    } else {
      group->resetLevel();
    }

    std::map<std::shared_ptr<const Group>, int> passed_groups;

    std::vector<std::set<std::shared_ptr<Group>>> affecting_groups;

    std::function<size_t(const std::shared_ptr<const Group> &)> fn =
        [&](const std::shared_ptr<const Group> &current) mutable -> size_t {
      if (auto it = passed_groups.find(current); it != passed_groups.end()) {
        return it->second;
      }
      if (current->isLevelOverriden()) {
        return -1;
      }
      if (current == group) {
        return 0;
      }
      if (not current->parent()) {
        return -1;
      }
      auto n = fn(current->parent());
      if (n == -1) {
        return -1;
      }
      affecting_groups[n].emplace(std::const_pointer_cast<Group>(current));
      return ++n;
    };

    for (const auto &it : groups_) {
      auto n = fn(it.second);
      passed_groups[it.second] = n;
    }

    for (const auto &stage : std::move(affecting_groups)) {
      for (const auto &changing_group : stage) {
        changing_group->setLevelFromGroup(changing_group->parent());
      }
    }

    for (auto it = loggers_.begin(); it != loggers_.end();) {
      if (auto logger = it->second.lock()) {
        if (auto it2 = passed_groups.find(logger->group());
            it2 != passed_groups.end()) {
          if (it2->second != -1) {
            logger->setGroup(logger->group());
          }
        }
        ++it;
      } else {
        it = loggers_.erase(it);
      }
    }
  }

  void LoggerSystem::setSinkOfLogger(
      const std::shared_ptr<Logger> &logger,
      std::optional<std::shared_ptr<Sink>> sink) {
    assert(logger != nullptr);

    if (sink) {
      logger->setSink(std::move(*sink));
    } else {
      logger->resetSink();
    }
  }

  void LoggerSystem::setLevelOfLogger(const std::shared_ptr<Logger> &logger,
                                      std::optional<Level> level) {
    assert(logger != nullptr);

    if (level) {
      logger->setLevel(*level);
    } else {
      logger->resetLevel();
    }
  }

  bool LoggerSystem::setParentOfGroup(const std::string &group_name,
                                      const std::string &parent_name) {
    auto it1 = groups_.find(group_name);
    if (it1 == groups_.end()) {
      return false;
    }
    auto &group = it1->second;

    auto it2 = groups_.find(parent_name);
    if (it2 == groups_.end()) {
      return false;
    }
    auto &parent = it2->second;

    // Check for recursion
    for (auto current = parent->parent(); current != nullptr;
         current = current->parent()) {
      if (current == group) {
        // Cyclic parentness is detected
        return false;
      }
    }

    setParentOfGroup(group, parent);
    return true;
  }

  bool LoggerSystem::setSinkOfGroup(const std::string &group_name,
                                    const std::string &sink_name) {
    auto sink = getSink(sink_name);
    if (not sink) {
      return false;
    }
    if (auto it = groups_.find(group_name); it != groups_.end()) {
      auto &group = it->second;
      setSinkOfGroup(group, std::move(sink));
      return true;
    }
    return false;
  }

  bool LoggerSystem::resetSinkOfGroup(const std::string &group_name) {
    if (auto it = groups_.find(group_name); it != groups_.end()) {
      auto &group = it->second;
      setSinkOfGroup(group, {});
      return true;
    }
    return false;
  }

  bool LoggerSystem::setLevelOfGroup(const std::string &group_name,
                                     Level level) {
    if (auto it = groups_.find(group_name); it != groups_.end()) {
      auto &group = it->second;
      setLevelOfGroup(group, level);
      return true;
    }
    return false;
  }

  bool LoggerSystem::resetLevelOfGroup(const std::string &group_name) {
    if (auto it = groups_.find(group_name); it != groups_.end()) {
      auto &group = it->second;
      setLevelOfGroup(group, {});
      return true;
    }
    return false;
  }

  bool LoggerSystem::setGroupOfLogger(const std::string &logger_name,
                                      const std::string &group_name) {
    if (auto group = getGroup(group_name)) {
      if (auto it = loggers_.find(logger_name); it != loggers_.end()) {
        if (auto logger = it->second.lock()) {
          logger->setGroup(std::move(group));
          return true;
        }
        loggers_.erase(it);
      }
    }
    return false;
  }

  bool LoggerSystem::setSinkOfLogger(const std::string &logger_name,
                                     const std::string &sink_name) {
    if (auto sink = getSink(sink_name)) {
      if (auto it = loggers_.find(logger_name); it != loggers_.end()) {
        if (auto logger = it->second.lock()) {
          logger->setSink(std::move(sink));
          return true;
        }
        loggers_.erase(it);
      }
    }
    return false;
  }

  bool LoggerSystem::resetSinkOfLogger(const std::string &logger_name) {
    if (auto it = loggers_.find(logger_name); it != loggers_.end()) {
      if (auto logger = it->second.lock()) {
        logger->setSinkFromGroup(logger->group());
        return true;
      }
    }
    return false;
  }

  bool LoggerSystem::setLevelOfLogger(const std::string &logger_name,
                                      Level level) {
    if (auto it = loggers_.find(logger_name); it != loggers_.end()) {
      if (auto logger = it->second.lock()) {
        logger->setLevel(level);
        return true;
      }
      loggers_.erase(it);
    }
    return false;
  }

  bool LoggerSystem::resetLevelOfLogger(const std::string &logger_name) {
    if (auto it = loggers_.find(logger_name); it != loggers_.end()) {
      if (auto logger = it->second.lock()) {
        logger->setLevelFromGroup(logger->group());
        return true;
      }
      loggers_.erase(it);
    }
    return false;
  }
}  // namespace soralog
