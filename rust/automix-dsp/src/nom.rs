use crate::math_utils::db_to_linear;

/// Number-of-open-microphones attenuation.
///
/// Applies -10*log10(NOM) dB of attenuation to compensate for the additive
/// noise of multiple open microphones.
#[derive(Debug, Clone)]
pub struct NomAttenuation {
    nom: f64,
    attenuation_linear: f64,
    attenuation_db: f64,
    enabled: bool,
}

impl Default for NomAttenuation {
    fn default() -> Self {
        Self::new()
    }
}

impl NomAttenuation {
    pub fn new() -> Self {
        Self {
            nom: 1.0,
            attenuation_linear: 1.0,
            attenuation_db: 0.0,
            enabled: true,
        }
    }

    /// Update with a new NOM count and recompute attenuation.
    pub fn update(&mut self, nom: f64) {
        self.nom = nom;
        if self.enabled && nom > 1.0 {
            self.attenuation_db = -10.0 * nom.log10();
            self.attenuation_linear = db_to_linear(self.attenuation_db);
        } else {
            self.attenuation_db = 0.0;
            self.attenuation_linear = 1.0;
        }
    }

    #[inline]
    pub fn linear(&self) -> f64 {
        self.attenuation_linear
    }

    #[inline]
    pub fn db(&self) -> f64 {
        self.attenuation_db
    }

    #[inline]
    pub fn nom(&self) -> f64 {
        self.nom
    }

    pub fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
        if !enabled {
            self.attenuation_db = 0.0;
            self.attenuation_linear = 1.0;
        }
    }

    #[inline]
    pub fn enabled(&self) -> bool {
        self.enabled
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use approx::assert_relative_eq;

    #[test]
    fn nom_one_no_attenuation() {
        let mut n = NomAttenuation::new();
        n.update(1.0);
        assert_relative_eq!(n.db(), 0.0, epsilon = 1e-10);
        assert_relative_eq!(n.linear(), 1.0, epsilon = 1e-10);
    }

    #[test]
    fn nom_two() {
        let mut n = NomAttenuation::new();
        n.update(2.0);
        // -10 * log10(2) = -3.0103 dB
        assert_relative_eq!(n.db(), -10.0 * 2.0_f64.log10(), epsilon = 1e-10);
    }

    #[test]
    fn nom_ten() {
        let mut n = NomAttenuation::new();
        n.update(10.0);
        assert_relative_eq!(n.db(), -10.0, epsilon = 1e-10);
    }

    #[test]
    fn disabled_no_attenuation() {
        let mut n = NomAttenuation::new();
        n.set_enabled(false);
        n.update(10.0);
        assert_relative_eq!(n.db(), 0.0, epsilon = 1e-10);
        assert_relative_eq!(n.linear(), 1.0, epsilon = 1e-10);
    }

    #[test]
    fn nom_less_than_one() {
        let mut n = NomAttenuation::new();
        n.update(0.5);
        // NOM < 1 should not apply attenuation
        assert_relative_eq!(n.db(), 0.0, epsilon = 1e-10);
        assert_relative_eq!(n.linear(), 1.0, epsilon = 1e-10);
    }

    #[test]
    fn nom_zero() {
        let mut n = NomAttenuation::new();
        n.update(0.0);
        // NOM = 0 is below threshold, no attenuation
        assert_relative_eq!(n.db(), 0.0, epsilon = 1e-10);
        assert_relative_eq!(n.linear(), 1.0, epsilon = 1e-10);
    }

    #[test]
    fn nom_negative() {
        let mut n = NomAttenuation::new();
        n.update(-1.0);
        // Negative NOM should not apply attenuation (< 1.0 guard)
        assert_relative_eq!(n.db(), 0.0, epsilon = 1e-10);
        assert_relative_eq!(n.linear(), 1.0, epsilon = 1e-10);
    }
}
