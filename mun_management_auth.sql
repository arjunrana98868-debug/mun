-- Unified MySQL Schema with Role-Based Authentication & Request Workflow
-- Compatible with MySQL 8.0+ and existing C++ Committee & Delegate modules

CREATE DATABASE IF NOT EXISTS `mun_management`
  DEFAULT CHARACTER SET utf8mb4
  COLLATE utf8mb4_0900_ai_ci;

USE `mun_management`;

-- ============================================================
-- 1. COMMITTEE & PORTFOLIO MODULE TABLES (C++ db.h compatible)
-- ============================================================

CREATE TABLE IF NOT EXISTS `committees` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(100) NOT NULL,
  `min_muns` INT NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_committee_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `portfolios` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `committee_id` INT NOT NULL,
  `name` VARCHAR(100) NOT NULL,
  `capacity` INT NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_committee_portfolio` (`committee_id`, `name`),
  CONSTRAINT `fk_portfolios_committee`
    FOREIGN KEY (`committee_id`) REFERENCES `committees` (`id`)
    ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `allocations` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `portfolio_id` INT NOT NULL,
  `delegate_name` VARCHAR(100) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_portfolio_delegate` (`portfolio_id`, `delegate_name`),
  CONSTRAINT `fk_allocations_portfolio`
    FOREIGN KEY (`portfolio_id`) REFERENCES `portfolios` (`id`)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- ============================================================
-- 2. DELEGATE MANAGEMENT TABLES (C++ Delegate.cpp compatible)
-- ============================================================

CREATE TABLE IF NOT EXISTS `delegates` (
  `delegate_id` INT NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(100) NOT NULL,
  `age` INT NOT NULL,
  `country` VARCHAR(100) NOT NULL,
  `mun_attend` INT NOT NULL DEFAULT 0,
  PRIMARY KEY (`delegate_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `previous_committees` (
  `committee_id` INT NOT NULL AUTO_INCREMENT,
  `delegate_id` INT NOT NULL,
  `committee_name` VARCHAR(150) NOT NULL,
  PRIMARY KEY (`committee_id`),
  CONSTRAINT `fk_prev_committees_delegate`
    FOREIGN KEY (`delegate_id`) REFERENCES `delegates` (`delegate_id`)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `award` (
  `award_id` INT NOT NULL AUTO_INCREMENT,
  `delegate_id` INT NOT NULL,
  `award_name` VARCHAR(150) NOT NULL,
  PRIMARY KEY (`award_id`),
  CONSTRAINT `fk_award_delegate`
    FOREIGN KEY (`delegate_id`) REFERENCES `delegates` (`delegate_id`)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- ============================================================
-- 3. AUTHENTICATION & ALLOCATION REQUEST WORKFLOW TABLES
-- ============================================================

CREATE TABLE IF NOT EXISTS `users` (
  `user_id` INT NOT NULL AUTO_INCREMENT,
  `username` VARCHAR(60) NOT NULL,
  `password_hash` VARCHAR(255) NOT NULL,
  `role` ENUM('admin', 'delegate') NOT NULL DEFAULT 'delegate',
  `full_name` VARCHAR(100) NOT NULL,
  `delegate_id` INT NULL,
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`user_id`),
  UNIQUE KEY `uq_users_username` (`username`),
  CONSTRAINT `fk_users_delegate`
    FOREIGN KEY (`delegate_id`) REFERENCES `delegates` (`delegate_id`)
    ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `allocation_requests` (
  `request_id` INT NOT NULL AUTO_INCREMENT,
  `user_id` INT NOT NULL,
  `delegate_id` INT NULL,
  `delegate_name` VARCHAR(100) NOT NULL,
  `portfolio_id` INT NOT NULL,
  `allocation_id` INT NULL,
  `request_type` ENUM('allocate', 'deallocate') NOT NULL,
  `status` ENUM('pending', 'approved', 'rejected') NOT NULL DEFAULT 'pending',
  `note` VARCHAR(255) DEFAULT '',
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  `reviewed_at` TIMESTAMP NULL,
  PRIMARY KEY (`request_id`),
  CONSTRAINT `fk_req_user`
    FOREIGN KEY (`user_id`) REFERENCES `users` (`user_id`)
    ON DELETE CASCADE,
  CONSTRAINT `fk_req_portfolio`
    FOREIGN KEY (`portfolio_id`) REFERENCES `portfolios` (`id`)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- ============================================================
-- 4. SEED DATA (Existing records + sample eligibility tiers)
--    Note: server.js automatically seeds default login accounts
--    (admin / admin123, aniket / user123, ayush / user123) on startup.
-- ============================================================

INSERT IGNORE INTO `committees` (`id`, `name`, `min_muns`) VALUES
  (2, 'UNGC', 0),
  (3, 'dit', 1),
  (4, 'UNSC', 3);

INSERT IGNORE INTO `portfolios` (`id`, `committee_id`, `name`, `capacity`) VALUES
  (3, 2, 'INDIA', 2),
  (4, 3, 'japan', 3),
  (5, 4, 'USA', 1),
  (6, 4, 'France', 2);

INSERT IGNORE INTO `allocations` (`id`, `portfolio_id`, `delegate_name`) VALUES
  (2, 3, 'Alexander'),
  (3, 4, 'arjun');

INSERT IGNORE INTO `delegates` (`delegate_id`, `name`, `age`, `country`, `mun_attend`) VALUES
  (1, 'Aniket KAushal', 20, 'India', 1),
  (2, 'Ayush', 12, 'India', 3),
  (3, 'Aryan Rastogi', 20, 'USA', 1),
  (4, 'Arjun Rana', 20, 'India', 1),
  (6, 'Ayush Sharma', 21, 'India', 0),
  (7, 'Ayush Pundir', 23, 'India', 1);

INSERT IGNORE INTO `previous_committees` (`committee_id`, `delegate_id`, `committee_name`) VALUES
  (1, 1, 'UNSA'),
  (2, 2, 'UNSA'),
  (3, 2, 'UNHRC'),
  (4, 3, 'UNHC'),
  (5, 4, 'UNHRC'),
  (6, 7, 'UNSC'),
  (7, 7, 'UNDA');

INSERT IGNORE INTO `award` (`award_id`, `delegate_id`, `award_name`) VALUES
  (1, 2, 'Best Delegate'),
  (2, 4, 'Best Delegate'),
  (3, 7, 'Best Speaker');