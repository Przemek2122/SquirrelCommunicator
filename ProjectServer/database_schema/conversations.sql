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
-- Struktura tabeli dla tabeli `conversations`
--

CREATE TABLE `conversations` (
  `conversation_id` bigint(20) UNSIGNED NOT NULL,
  `last_message_at` timestamp NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Zrzut danych tabeli `conversations`
--

INSERT INTO `conversations` (`conversation_id`, `last_message_at`) VALUES
(4, '2025-11-21 19:47:23'),
(5, '2025-11-21 19:52:16');

--
-- Indeksy dla zrzutów tabel
--

--
-- Indeksy dla tabeli `conversations`
--
ALTER TABLE `conversations`
  ADD PRIMARY KEY (`conversation_id`);

--
-- AUTO_INCREMENT dla zrzuconych tabel
--

--
-- AUTO_INCREMENT dla tabeli `conversations`
--
ALTER TABLE `conversations`
  MODIFY `conversation_id` bigint(20) UNSIGNED NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=6;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
