-- phpMyAdmin SQL Dump
-- version 5.0.4deb2+deb11u2
-- https://www.phpmyadmin.net/
--
-- Host: localhost:3306
-- Czas generowania: 14 Lis 2025, 15:13
-- Wersja serwera: 10.5.29-MariaDB-0+deb11u1
-- Wersja PHP: 7.4.33

SET FOREIGN_KEY_CHECKS=0;
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
CREATE DATABASE IF NOT EXISTS `sqrllapitest` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE `sqrllapitest`;

-- --------------------------------------------------------

--
-- Struktura tabeli dla tabeli `messages`
--

DROP TABLE IF EXISTS `messages`;
CREATE TABLE `messages` (
  `id` bigint(20) UNSIGNED NOT NULL,
  `receiver_id` bigint(20) UNSIGNED NOT NULL,
  `sender_id` bigint(20) UNSIGNED NOT NULL,
  `text` varchar(4096) NOT NULL,
  `created_at` date NOT NULL DEFAULT current_timestamp(),
  `read_at` date DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Struktura tabeli dla tabeli `users`
--

DROP TABLE IF EXISTS `users`;
CREATE TABLE `users` (
  `id` bigint(20) UNSIGNED NOT NULL,
  `username` varchar(256) NOT NULL,
  `password` varchar(2048) NOT NULL,
  `email` varchar(256) NOT NULL,
  `displayedname` varchar(256) NOT NULL,
  `lastactive` date DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Zrzut danych tabeli `users`
--

INSERT INTO `users` (`id`, `username`, `password`, `email`, `displayedname`, `lastactive`) VALUES
(2, 'Przemysław Wiewióra', '$argon2id$v=19$m=16384,t=2,p=1$fdiNx7qY3RfSvBoRxcrEyg$ZS4hufqcxBajNpfuOEQV4VjU8FlCK2ZuiZp6NLj2Al4\0', 'przemek1234wiewiora@gmail.com', 'Przemysław Wiewióra', NULL),
(3, 'SuperStrong123!', '$argon2id$v=19$m=16384,t=2,p=1$ov/3mXCQpvr45jRhZAauow$gtAQhQ1gjSrX+/SCyWbTK1aUq764ZcWOuvM5BGbZK/E\0', 'bob@bobrowniczy.com', 'SuperStrong123!', '2025-11-14'),
(4, 'postmanuser', '$argon2id$v=19$m=16384,t=2,p=1$YouU1ASS77+eK9AN0lZDnA$t/lgl2gPfQ7xm/hSVA1tfAN7gkYwm9fLq4QnCRQzhb8\0', 'postmanuser@sqrll.net', 'postmanuser', '2025-11-06');

--
-- Indeksy dla zrzutów tabel
--

--
-- Indeksy dla tabeli `messages`
--
ALTER TABLE `messages`
  ADD PRIMARY KEY (`id`),
  ADD KEY `sender_id` (`sender_id`),
  ADD KEY `receiver_id` (`receiver_id`),
  ADD KEY `idx_sender` (`sender_id`),
  ADD KEY `idx_receiver` (`receiver_id`);

--
-- Indeksy dla tabeli `users`
--
ALTER TABLE `users`
  ADD PRIMARY KEY (`id`);

--
-- AUTO_INCREMENT dla zrzuconych tabel
--

--
-- AUTO_INCREMENT dla tabeli `messages`
--
ALTER TABLE `messages`
  MODIFY `id` bigint(20) UNSIGNED NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT dla tabeli `users`
--
ALTER TABLE `users`
  MODIFY `id` bigint(20) UNSIGNED NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=5;

--
-- Ograniczenia dla zrzutów tabel
--

--
-- Ograniczenia dla tabeli `messages`
--
ALTER TABLE `messages`
  ADD CONSTRAINT `fk_receiver` FOREIGN KEY (`receiver_id`) REFERENCES `users` (`id`),
  ADD CONSTRAINT `fk_sender` FOREIGN KEY (`sender_id`) REFERENCES `users` (`id`);
SET FOREIGN_KEY_CHECKS=1;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
