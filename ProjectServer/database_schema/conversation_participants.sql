-- phpMyAdmin SQL Dump
-- version 5.0.4deb2+deb11u2
-- https://www.phpmyadmin.net/
--
-- Host: localhost:3306
-- Czas generowania: 21 Lis 2025, 20:53
-- Wersja serwera: 10.5.29-MariaDB-0+deb11u1
-- Wersja PHP: 7.4.33

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Baza danych: `sqrllapitest`
--

-- --------------------------------------------------------

--
-- Struktura tabeli dla tabeli `conversation_participants`
--

CREATE TABLE `conversation_participants` (
  `conversation_participant_id` bigint(20) UNSIGNED NOT NULL,
  `conversation_id` bigint(20) UNSIGNED NOT NULL,
  `user_id` bigint(20) UNSIGNED NOT NULL,
  `last_read_message_id` bigint(20) UNSIGNED DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Indeksy dla zrzutów tabel
--

--
-- Indeksy dla tabeli `conversation_participants`
--
ALTER TABLE `conversation_participants`
  ADD PRIMARY KEY (`conversation_participant_id`),
  ADD KEY `conversation_id` (`conversation_id`),
  ADD KEY `user_id` (`user_id`),
  ADD KEY `last_read_message_id` (`last_read_message_id`);

--
-- Ograniczenia dla zrzutów tabel
--

--
-- Ograniczenia dla tabeli `conversation_participants`
--
ALTER TABLE `conversation_participants`
  ADD CONSTRAINT `conversation_participants_ibfk_1` FOREIGN KEY (`conversation_id`) REFERENCES `conversations` (`conversation_id`),
  ADD CONSTRAINT `conversation_participants_ibfk_2` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`),
  ADD CONSTRAINT `conversation_participants_ibfk_3` FOREIGN KEY (`last_read_message_id`) REFERENCES `messages` (`id`);
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
