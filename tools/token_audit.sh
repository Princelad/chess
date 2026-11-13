#!/bin/sh
# 17.5 token audit: flags raw sf::Color literals that bypass the design
# tokens (client/src/ui/theme.h). Exempt files are the documented
# non-consumers / semantic owners:
#   ui/theme.h        - the token definitions themselves
#   themes.h          - board palettes (14.4 board semantics)
#   boardview.cpp     - board rendering (14.1 exclusion, 15.5)
#   chat_log.h        - chat name palette (11.8)
#   spinner.h         - motion polish alpha fades (15.5)
#   avatars.h         - curated identicon deep/light pairs (14.3)
#   player_card.*     - clock heat colors carried verbatim from 11.1
#   rounded_rect.h    - shape geometry, no palette
# Constructions derived FROM tokens (sf::Color(ui::pal::x.r, ...)) pass.
#
# Debug-only: enabled with -DCHESS_TOKEN_AUDIT=ON (target `token-audit`).

cd "$(dirname "$0")/.." || exit 1

violations=$(grep -rn 'sf::Color(' client/src \
    --include='*.cpp' --include='*.h' \
    | grep -v 'ui/pal\|ui::pal\|pal::' \
    | grep -vE 'theme\.h|themes\.h|boardview\.cpp|chat_log\.h|spinner\.h|avatars\.h|player_card\.|rounded_rect\.h' \
    | grep -v 'sf::Color::')

if [ -n "$violations" ]; then
    echo "Token violations (raw sf::Color literals outside theme.h):"
    echo "$violations"
    exit 1
fi
echo "token audit clean"
