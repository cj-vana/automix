use crate::math_utils::db_to_linear;

/// Number-of-open-microphones attenuation.
///
/// The classic -10*log10(NOM) dB compensation, which belongs to *gating*
/// automixers: there every open mic sits at unity, so N of them raise the bus
/// by 10*log10(N) and it has to be taken back out.
///
/// Gain sharing already does that job. Because the per-channel gains are
/// normalised to sum to 1.0, the loop gain is constant no matter how many mics
/// are open, so applying this on top attenuates a second time — with four
/// talkers the mix lands 6 dB lower than it should, and the error grows with
/// the number of open mics. That is exactly the level pumping an automixer is
/// supposed to prevent, so it is off by default and the value below is computed
/// for the meter whether or not it is applied.
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
            enabled: false,
        }
    }

    /// Update with a new NOM count and recompute the attenuation figure.
    ///
    /// The figure is always computed so the meter can report it; whether it
    /// reaches the audio path is `enabled`'s business.
    pub fn update(&mut self, nom: f64) {
        self.nom = nom;
        if nom > 1.0 {
            self.attenuation_db = -10.0 * nom.log10();
            self.attenuation_linear = db_to_linear(self.attenuation_db);
        } else {
            self.attenuation_db = 0.0;
            self.attenuation_linear = 1.0;
        }
    }

    /// Linear gain to apply in the audio path: unity unless explicitly enabled.
    #[inline]
    pub fn linear(&self) -> f64 {
        if self.enabled {
            self.attenuation_linear
        } else {
            1.0
        }
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
        n.set_enabled(true);
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
    fn disabled_by_default_so_gain_sharing_is_not_double_attenuated() {
        let mut n = NomAttenuation::new();
        n.update(10.0);
        // The figure is still reported for the meter...
        assert_relative_eq!(n.db(), -10.0, epsilon = 1e-10);
        // ...but nothing reaches the audio path.
        assert_relative_eq!(n.linear(), 1.0, epsilon = 1e-10);
    }

    #[test]
    fn enabling_applies_attenuation_to_the_audio_path() {
        let mut n = NomAttenuation::new();
        n.update(4.0);
        assert_relative_eq!(n.linear(), 1.0, epsilon = 1e-10);

        n.set_enabled(true);
        assert_relative_eq!(
            n.linear(),
            db_to_linear(-10.0 * 4.0_f64.log10()),
            epsilon = 1e-10
        );

        n.set_enabled(false);
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
