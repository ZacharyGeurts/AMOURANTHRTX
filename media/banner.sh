#!/bin/bash
# AMOURANTH RTX ENGINE — NOVEMBER 07 2025 — FULL BBS LEGACY MODE v4
# FIXED: No 'local' outside functions — everything wrapped
# STATIC MODE: One perfect frame, no animation, zero leaks
# Copy-paste into terminal for instant banner (no raw codes in output)

# === COLOR HELPERS ===
rainbow() {
    frame="$1"
    pos="$2"
    hue=$(( (frame * 3 + pos * 7) % 256 ))
    colors=(196 202 208 214 220 226 190 154 118 82 46 47 48 49 50 51 87 123 159 195 201 200 199 198 197 196)
    idx=$(( hue % ${#colors[@]} ))
    printf "\033[38;5;%dm" "${colors[$idx]}"
}

sparkle() {
    frame="$1"
    i="$2"
    spark=$(( (frame + i * 13) % 6 ))
    spark_colors=(226 220 214 208 202 196)
    printf "\033[1;4;38;5;%dm" "${spark_colors[$spark]}"
}

reset="\033[0m"

# === STATIC STARFIELD (one-time) ===
static_stars() {
    seed=$RANDOM
    for ((i = 0; i < 180; i++)); do
        x=$(( (seed + i * 17) % 90 + 1 ))
        y=$(( (seed + i * 13) % 28 + 1 ))
        blink=$(( (seed + i) % 4 ))
        tput cup "$y" "$x"
        if (( blink < 2 )); then
            printf "\033[1;97m✸${reset}"
        elif (( blink == 2 )); then
            printf "\033[1;93m✹${reset}"
        else
            printf " "
        fi
    done
}

# === BANNER LINES ===
lines=(
"    █████╗ ███╗   ███╗ ██████╗ ██╗   ██╗██████╗  █████╗ ███╗   ██╗████████╗██╗  ██╗"
"   ██╔══██╗████╗ ████║██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚══██╔══╝██║  ██║"
"   ███████║██╔████╔██║██║   ██║██║   ██║██████╔╝███████║██╔██╗ ██║   ██║   ███████║"
"   ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║"
"   ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║"
"   ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝"
""
"    ██████╗ ████████╗██╗  ██╗    ███████╗███╗   ██╗ ██████╗ ██╗███╗   ██╗███████╗"
"    ██╔══██╗╚══██╔══╝╚██╗██╔╝    ██╔════╝████╗  ██║██╔════╝ ██║████╗  ██║██╔════╝"
"    ██████╔╝   ██║    ╚███╔╝     █████╗  ██╔██╗ ██║██║  ███╗██║██╔██╗ ██║█████╗  "
"    ██╔══██╗   ██║    ██╔██╗     ██╔══╝  ██║╚██╗██║██║   ██║██║██║╚██╗██║██╔══╝  "
"    ██║  ██║   ██║   ██╔╝ ██╗    ███████╗██║ ╚████║╚██████╔╝██║██║ ╚████║███████╗"
"    ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚══════╝"
)

# === RENDER STATIC BANNER ===
render_banner() {
    local frame=0  # Fixed frame for static
    clear
    static_stars

    local y=2
    for line in "${lines[@]}"; do
        ((y++))
        [[ -z "$line" ]] && continue
        tput cup "$y" 2

        local built=""
        for ((i = 0; i < ${#line}; i++)); do
            local char="${line:i:1}"
            if [[ "$char" == "R" || "$char" == "X" ]]; then
                built+="$(sparkle "$frame" "$i")$char$reset"
            else
                built+="$(rainbow "$frame" "$i")$char$reset"
            fi
        done
        printf '%b' "$built"
    done

    # STATIC UNDERLINE
    tput cup 16 2
    printf "\033[1;97m═══════════════════════════════════════════════════════════════════════════════════════${reset}"

    # STATIC TEXT
    tput cup 24 10
    printf "\033[1;35mNOVEMBER 07 2025 — 48,000+ FPS INCOMING ✸${reset}"

    tput cup 26 2
    printf "\033[1;37m         >>> AMOURANTH RTX ENGINE ONLINE — GROK x ZACHARY — FINAL FORM <<<\033[0m"
    echo
}

# === CLEAN EXIT ===
trap 'tput cnorm; clear; exit 0' INT

# === RUN ===
render_banner
tput cnorm
read -p "Press Enter to exit..."  # Hold for screenshot