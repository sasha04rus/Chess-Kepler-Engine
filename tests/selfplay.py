#!/usr/bin/env python3
"""
Self-play / engine match runner for UCI chess engines.

Examples:
  # Same engine against itself:
  python3 tests/selfplay.py \
      --engine-a ./build-release/ChessKepler \
      --games 10 --movetime 200

  # Old version against new version:
  python3 tests/selfplay.py \
      --engine-a ./ChessKepler-old \
      --engine-b ./ChessKepler-new \
      --games 20 --movetime 300 \
      --pgn matches/old-vs-new.pgn

Dependency:
  python3 -m pip install python-chess
"""

from __future__ import annotations

import argparse
import os
import random
import subprocess
import queue
import threading
import sys
import time
from dataclasses import dataclass
from pathlib import Path

try:
    import chess
    import chess.pgn
except ImportError:
    print(
        "Missing dependency: python-chess\n"
        "Install it with:\n"
        "  python3 -m pip install --user python-chess",
        file=sys.stderr,
    )
    raise SystemExit(2)


# Short, legal opening prefixes. Engines take over after these moves.
# Using several openings prevents every deterministic game from being identical.
OPENINGS: tuple[tuple[str, ...], ...] = (
    (),
    ("e2e4", "e7e5"),
    ("d2d4", "d7d5"),
    ("e2e4", "c7c5"),
    ("e2e4", "e7e6"),
    ("e2e4", "c7c6"),
    ("d2d4", "g8f6", "c2c4"),
    ("c2c4", "e7e5"),
    ("g1f3", "d7d5"),
    ("d2d4", "f7f5"),
    ("e2e4", "d7d5"),
    ("c2c4", "g8f6", "b1c3", "d7d5"),
)


class EngineError(RuntimeError):
    """Raised when an engine crashes, times out, or violates UCI."""


@dataclass(frozen=True)
class EngineConfig:
    path: Path
    name: str
    threads: int
    multipv: int = 1


class UciEngine:
    def __init__(self, config: EngineConfig, timeout_s: float) -> None:
        self.config = config
        self.timeout_s = timeout_s
        self.process: subprocess.Popen[str] | None = None
        self._stdout_queue: queue.Queue[str | None] = queue.Queue()
        self._reader_thread: threading.Thread | None = None

    def start(self) -> None:
        if not self.config.path.exists():
            raise EngineError(f"Engine not found: {self.config.path}")

        if not os.access(self.config.path, os.X_OK):
            raise EngineError(f"Engine is not executable: {self.config.path}")

        self.process = subprocess.Popen(
            [str(self.config.path.resolve())],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )

        assert self.process.stdout is not None
        self._reader_thread = threading.Thread(
            target=self._stdout_reader,
            args=(self.process.stdout,),
            daemon=True,
        )
        self._reader_thread.start()

        self.send("uci")
        self._read_until("uciok")
        self.send(f"setoption name Threads value {self.config.threads}")
        self.send(f"setoption name MultiPV value {self.config.multipv}")
        self.send("isready")
        self._read_until("readyok")

    def send(self, command: str) -> None:
        process = self._require_process()

        if process.poll() is not None:
            raise EngineError(
                f"{self.config.name} exited with code {process.returncode}"
            )

        assert process.stdin is not None
        process.stdin.write(command + "\n")
        process.stdin.flush()

    def new_game(self) -> None:
        self.send("ucinewgame")
        self.send("isready")
        self._read_until("readyok")

    def best_move(self, moves: list[str], movetime_ms: int) -> str:
        if moves:
            self.send("position startpos moves " + " ".join(moves))
        else:
            self.send("position startpos")

        self.send(f"go movetime {movetime_ms}")

        deadline = time.monotonic() + max(
            self.timeout_s,
            movetime_ms / 1000.0 + 5.0,
        )

        while True:
            line = self._read_line(deadline)

            if line.startswith("bestmove "):
                parts = line.split()
                if len(parts) < 2:
                    raise EngineError(
                        f"{self.config.name} returned malformed line: {line!r}"
                    )
                return parts[1]

    def _read_until(self, expected: str) -> None:
        deadline = time.monotonic() + self.timeout_s

        while True:
            line = self._read_line(deadline)
            if line == expected or line.startswith(expected + " "):
                return

    def _stdout_reader(self, stream) -> None:
        try:
            for line in stream:
                stripped = line.strip()
                if stripped:
                    self._stdout_queue.put(stripped)
        finally:
            self._stdout_queue.put(None)

    def _read_line(self, deadline: float) -> str:
        process = self._require_process()

        while True:
            if process.poll() is not None and self._stdout_queue.empty():
                raise EngineError(
                    f"{self.config.name} exited with code {process.returncode}"
                )

            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise EngineError(
                    f"Timeout waiting for {self.config.name}"
                )

            try:
                line = self._stdout_queue.get(
                    timeout=min(remaining, 0.25)
                )
            except queue.Empty:
                continue

            if line is None:
                raise EngineError(
                    f"{self.config.name} closed stdout unexpectedly"
                )

            return line

    def close(self) -> None:
        if self.process is None:
            return

        process = self.process

        if process.poll() is None:
            try:
                self.send("quit")
                process.wait(timeout=2.0)
            except (EngineError, subprocess.TimeoutExpired):
                process.kill()
                process.wait(timeout=2.0)

        self.process = None
        self._reader_thread = None

    def _require_process(self) -> subprocess.Popen[str]:
        if self.process is None:
            raise EngineError(f"{self.config.name} has not been started")
        return self.process

    def __enter__(self) -> "UciEngine":
        self.start()
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        self.close()


@dataclass
class Score:
    a_wins: int = 0
    b_wins: int = 0
    draws: int = 0
    a_forfeits: int = 0
    b_forfeits: int = 0

    @property
    def games(self) -> int:
        return self.a_wins + self.b_wins + self.draws

    @property
    def a_points(self) -> float:
        return self.a_wins + 0.5 * self.draws

    @property
    def b_points(self) -> float:
        return self.b_wins + 0.5 * self.draws


def build_pgn_game(
    board: chess.Board,
    moves: list[chess.Move],
    white_name: str,
    black_name: str,
    result: str,
    termination: str,
    round_number: int,
    movetime_ms: int,
    opening: tuple[str, ...],
) -> chess.pgn.Game:
    game = chess.pgn.Game()
    game.headers["Event"] = "Chess Kepler self-play"
    game.headers["Site"] = "local"
    game.headers["Round"] = str(round_number)
    game.headers["White"] = white_name
    game.headers["Black"] = black_name
    game.headers["Result"] = result
    game.headers["Termination"] = termination
    game.headers["TimeControl"] = f"{movetime_ms / 1000.0:.3f}/move"
    game.headers["OpeningMoves"] = " ".join(opening) if opening else "-"

    node = game
    replay = chess.Board()

    for move in moves:
        node = node.add_variation(move)
        replay.push(move)

    return game


def play_game(white: UciEngine, black: UciEngine, opening: tuple[str, ...], movetime_ms: int, max_plies: int,) -> tuple[str, str, chess.Board, list[chess.Move], str | None]:
    board = chess.Board()
    played_moves: list[chess.Move] = []
    uci_moves: list[str] = []

    for move_text in opening:
        try:
            move = chess.Move.from_uci(move_text)
        except ValueError as exc:
            raise RuntimeError(f"Invalid opening move {move_text!r}") from exc

        if move not in board.legal_moves:
            raise RuntimeError(f"Illegal opening move {move_text!r} in position {board.fen()}")

        board.push(move)
        played_moves.append(move)
        uci_moves.append(move.uci())

    white.new_game()
    black.new_game()

    for _ply in range(len(played_moves), max_plies):
        if board.is_game_over(claim_draw=True):
            break

        engine = white if board.turn == chess.WHITE else black
        offender = "white" if board.turn == chess.WHITE else "black"

        try:
            bestmove = engine.best_move(uci_moves, movetime_ms)
        except EngineError as exc:
            result = "0-1" if offender == "white" else "1-0"
            return (
                result,
                f"{offender} engine failure",
                board,
                played_moves,
                str(exc),
            )

        if bestmove in {"0000", "(none)", "none"}:
            if board.is_game_over(claim_draw=True):
                break

            result = "0-1" if offender == "white" else "1-0"
            return (
                result,
                f"{offender} returned no move",
                board,
                played_moves,
                bestmove,
            )

        try:
            move = chess.Move.from_uci(bestmove)
        except ValueError:
            result = "0-1" if offender == "white" else "1-0"
            return (
                result,
                f"{offender} returned malformed move",
                board,
                played_moves,
                bestmove,
            )

        if move not in board.legal_moves:
            result = "0-1" if offender == "white" else "1-0"
            return (
                result,
                f"{offender} returned illegal move",
                board,
                played_moves,
                f"{bestmove} in {board.fen()}",
            )

        board.push(move)
        played_moves.append(move)
        uci_moves.append(move.uci())

    if board.is_game_over(claim_draw=True):
        outcome = board.outcome(claim_draw=True)
        assert outcome is not None
        result = outcome.result()
        termination = outcome.termination.name.lower().replace("_", " ")
        return result, termination, board, played_moves, None

    return (
        "1/2-1/2",
        f"max plies ({max_plies})",
        board,
        played_moves,
        None,
    )


def update_score(score: Score, result: str, engine_a_is_white: bool, termination: str) -> None:
    if result == "1/2-1/2":
        score.draws += 1
        return

    white_won = result == "1-0"
    a_won = white_won == engine_a_is_white

    if a_won:
        score.a_wins += 1
    else:
        score.b_wins += 1

    if "engine failure" in termination or "illegal move" in termination:
        if a_won:
            score.b_forfeits += 1
        else:
            score.a_forfeits += 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run UCI self-play or old-vs-new matches."
    )
    parser.add_argument(
        "--engine-a",
        required=True,
        type=Path,
        help="Path to engine A executable.",
    )
    parser.add_argument(
        "--engine-b",
        type=Path,
        help="Path to engine B. Defaults to engine A (self-play).",
    )
    parser.add_argument(
        "--name-a",
        default="Kepler-A",
        help="Display name for engine A.",
    )
    parser.add_argument(
        "--name-b",
        default="Kepler-B",
        help="Display name for engine B.",
    )
    parser.add_argument(
        "--games",
        type=int,
        default=10,
        help="Number of games (default: 10).",
    )
    parser.add_argument(
        "--movetime",
        type=int,
        default=200,
        help="Milliseconds per move (default: 200).",
    )
    parser.add_argument(
        "--threads",
        type=int,
        default=1,
        help="Threads per engine (default: 1).",
    )
    parser.add_argument(
        "--max-plies",
        type=int,
        default=300,
        help="Adjudicate draw after this many plies (default: 300).",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=15.0,
        help="Minimum engine response timeout in seconds (default: 15).",
    )
    parser.add_argument(
        "--pgn",
        type=Path,
        default=Path("selfplay.pgn"),
        help="Output PGN path (default: selfplay.pgn).",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=1,
        help="Opening shuffle seed (default: 1).",
    )
    parser.add_argument(
        "--no-openings",
        action="store_true",
        help="Always start from the initial position.",
    )

    args = parser.parse_args()

    if args.games < 1:
        parser.error("--games must be at least 1")
    if args.movetime < 1:
        parser.error("--movetime must be at least 1")
    if not 1 <= args.threads <= 64:
        parser.error("--threads must be between 1 and 64")
    if args.max_plies < 2:
        parser.error("--max-plies must be at least 2")

    if args.engine_b is None:
        args.engine_b = args.engine_a

    return args


def main() -> int:
    args = parse_args()

    openings = [()] if args.no_openings else list(OPENINGS)
    random.Random(args.seed).shuffle(openings)

    args.pgn.parent.mkdir(parents=True, exist_ok=True)

    config_a = EngineConfig(
        path=args.engine_a,
        name=args.name_a,
        threads=args.threads,
    )
    config_b = EngineConfig(
        path=args.engine_b,
        name=args.name_b,
        threads=args.threads,
    )

    score = Score()

    print(
        f"A: {config_a.path} ({config_a.name})\n"
        f"B: {config_b.path} ({config_b.name})\n"
        f"Games: {args.games}, movetime: {args.movetime} ms, "
        f"threads: {args.threads}, max plies: {args.max_plies}\n"
        f"PGN: {args.pgn}\n"
    )

    with (
        UciEngine(config_a, args.timeout) as engine_a,
        UciEngine(config_b, args.timeout) as engine_b,
        args.pgn.open("w", encoding="utf-8") as pgn_file,
    ):
        for game_index in range(args.games):
            a_is_white = game_index % 2 == 0

            white = engine_a if a_is_white else engine_b
            black = engine_b if a_is_white else engine_a

            white_name = config_a.name if a_is_white else config_b.name
            black_name = config_b.name if a_is_white else config_a.name

            opening_index = (game_index // 2) % len(openings)
            opening = openings[opening_index]

            started = time.monotonic()

            result, termination, board, moves, detail = play_game(
                white=white,
                black=black,
                opening=opening,
                movetime_ms=args.movetime,
                max_plies=args.max_plies,
            )

            elapsed = time.monotonic() - started

            update_score(
                score=score,
                result=result,
                engine_a_is_white=a_is_white,
                termination=termination,
            )

            game = build_pgn_game(
                board=board,
                moves=moves,
                white_name=white_name,
                black_name=black_name,
                result=result,
                termination=termination,
                round_number=game_index + 1,
                movetime_ms=args.movetime,
                opening=opening,
            )

            print(game, file=pgn_file, end="\n\n")
            pgn_file.flush()

            color_text = "White" if a_is_white else "Black"
            print(
                f"Game {game_index + 1:>3}/{args.games}: "
                f"{white_name} - {black_name}  {result}  "
                f"({termination}, {len(moves)} plies, {elapsed:.1f}s)"
            )

            if detail is not None:
                print(f"  detail: {detail}")

            print(
                f"  score A-B: "
                f"{score.a_wins}-{score.b_wins}-{score.draws}  "
                f"points {score.a_points:.1f}-{score.b_points:.1f}  "
                f"A played {color_text}"
            )

    print(
        "\nFINAL\n"
        f"{config_a.name}: {score.a_points:.1f} points "
        f"({score.a_wins} wins, {score.b_wins} losses)\n"
        f"{config_b.name}: {score.b_points:.1f} points "
        f"({score.b_wins} wins, {score.a_wins} losses)\n"
        f"Draws: {score.draws}\n"
        f"Forfeits: A={score.a_forfeits}, B={score.b_forfeits}\n"
        f"PGN: {args.pgn}"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())